/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "powerdevilapp.h"

#include "powerdevilfdoconnector.h"
#include "powermanagementadaptor.h"
#include "powermanagementpolicyagentadaptor.h"

#include "powerdevilcore.h"
#include "powerdevil_debug.h"
#include "powerdevil_version.h"

#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <QFileInfo>
#include <QAction>
#include <QKeySequence>
#include <QSessionManager>

#include <KCrash>
#include <KDBusService>
#include <KAboutData>
#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>

#include <kworkspace.h>

#include "PowerDevilProfileSettings.h"

PowerDevilApp::PowerDevilApp(int &argc, char **argv)
    : QGuiApplication(argc, argv)
    , m_core(nullptr)
{

}

PowerDevilApp::~PowerDevilApp()
{
    delete m_core;
}

static void migrateConfig() {
    KSharedConfigPtr powerDevilConfig = KSharedConfig::openConfig(QStringLiteral("powermanagementprofilesrc"));

    auto migrationInfo = powerDevilConfig->group("migration");
    if (migrationInfo.hasKey("MigratedToKConfigXT")) {
        qDebug() << "Configuration already migrated";
        return;
    }
    migrationInfo.writeEntry("MigratedToKConfigXT", true);

    // Top level groups are the groups that holds all the configuration.
    for (const QString& profileConfig : powerDevilConfig->groupList()) {
        // Important, oldConfig and newConfig are being read from the same file,
        // so we can't write on the oldConfig at all. just read the data and move
        // it to the newConfig when needed.

        // This method is needed because the old configuration checked if
        // a group existed to see if we should use it or not, but that's
        // impossible on kconfigxt, the groups always exists.

        // All the other keys use the same name as the previous configuration
        powerDevilConfig->sync();
        const auto oldConfig = powerDevilConfig->group(profileConfig);
        PowerDevilProfileSettings newConfig(profileConfig);
        qDebug() << "Porting" << profileConfig;

        if (oldConfig.hasGroup(QStringLiteral("KeyboardBrightnessControl"))) {
            newConfig.setManageKeyboardBrightness(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("HandleButtonEvents"))) {
            newConfig.setManageButtonEvents(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("SuspendSession"))) {
            newConfig.setManageSuspendSession(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("DimDisplay"))) {
            newConfig.setManageDimDisplay(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("KernelPowerProfile"))) {
            newConfig.setManageKernelPowerProfile(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("DPMSControl"))) {
            qDebug() << "It has DPMSControl, setting to true";
            newConfig.setManageDPMS(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("BrightnessControl"))) {
            newConfig.setBrightnessControl(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("RunScript"))) {
            newConfig.setManageRunScript(true);
        }
        if (oldConfig.hasGroup(QStringLiteral("WirelessPowerSaving"))) {
            newConfig.setManageWirelessPowerSaving(true);
        }
        newConfig.save();
    }
}

void PowerDevilApp::init()
{
    KLocalizedString::setApplicationDomain("powerdevil");

    KAboutData aboutData(QStringLiteral("org_kde_powerdevil"), i18n("KDE Power Management System"), QStringLiteral(POWERDEVIL_VERSION_STRING),
                         i18nc("@title", "PowerDevil, an advanced, modular and lightweight power management daemon"),
                         KAboutLicense::GPL,
                         i18nc("@info:credit", "(c) 2015-2019 Kai Uwe Broulik"));
    aboutData.addAuthor(i18nc("@info:credit", "Kai Uwe Broulik"),
                        i18nc("@info:credit", "Maintainer"),
                        QStringLiteral("kde@privat.broulik.de"));
    aboutData.addAuthor(i18nc("@info:credit", "Dario Freddi"),
                        i18nc("@info:credit", "Previous maintainer"),
                        QStringLiteral("drf@kde.org"));
    aboutData.setProductName("Powerdevil");

    KAboutData::setApplicationData(aboutData);

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("org.freedesktop.PowerManagement")) ||
        QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("com.novell.powersave")) ||
        QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("org.freedesktop.Policy.Power"))) {
        qCCritical(POWERDEVIL) << "KDE Power Management system not initialized, another power manager has been detected";
        return;
    }

    migrateConfig();

    // not parenting Core to PowerDevilApp as it is the deleted too late on teardown
    // where the X connection is already lost leading to a crash (Bug 371127)
    m_core = new PowerDevil::Core(nullptr/*, KComponentData(aboutData)*/);

    connect(m_core, &PowerDevil::Core::coreReady, this, &PowerDevilApp::onCoreReady);

    // Before doing anything, let's set up our backend
    const QStringList paths = QCoreApplication::libraryPaths();
    QFileInfoList fileInfos;
    for (const QString &path : paths) {
        QDir dir(path + QLatin1String("/kf5/powerdevil/"),
            QStringLiteral("*"),
            QDir::SortFlags(QDir::QDir::Name),
            QDir::NoDotAndDotDot | QDir::Files);
        fileInfos.append(dir.entryInfoList());
    }

    QFileInfo backendFileInfo;
    for (const QFileInfo &f : qAsConst(fileInfos)) {
        if (f.baseName().toLower() == QLatin1String("powerdevilupowerbackend")) {
            backendFileInfo = f;
            break;
        }
    }

    QPluginLoader *loader = new QPluginLoader(backendFileInfo.filePath(), m_core);
    QObject *instance = loader->instance();
    if (!instance) {
        qCDebug(POWERDEVIL) << loader->errorString();
        qCCritical(POWERDEVIL) << "KDE Power Management System init failed!";
        m_core->loadCore(nullptr);
        return;
    }

    auto interface = qobject_cast<PowerDevil::BackendInterface*>(instance);
    if (!interface) {
        qCDebug(POWERDEVIL) << "Failed to cast plugin instance to BackendInterface, check your plugin";
        qCCritical(POWERDEVIL) << "KDE Power Management System init failed!";
        m_core->loadCore(nullptr);
        return;
    }

    qCDebug(POWERDEVIL) << "Backend loaded, loading core";
    m_core->loadCore(interface);
}

void PowerDevilApp::onCoreReady()
{
    qCDebug(POWERDEVIL) << "Core is ready, registering various services on the bus...";
    //DBus logic for the core
    new PowerManagementAdaptor(m_core);
    new PowerDevil::FdoConnector(m_core);

    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.Solid.PowerManagement"));
    QDBusConnection::sessionBus().registerObject(QLatin1String("/org/kde/Solid/PowerManagement"), m_core);

    QDBusConnection::systemBus().interface()->registerService("org.freedesktop.Policy.Power");

    // Start the Policy Agent service
    qDBusRegisterMetaType<QList<InhibitionInfo>>();
    qDBusRegisterMetaType<InhibitionInfo>();
    new PowerManagementPolicyAgentAdaptor(PowerDevil::PolicyAgent::instance());

    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.Solid.PowerManagement.PolicyAgent"));
    QDBusConnection::sessionBus().registerObject(QLatin1String("/org/kde/Solid/PowerManagement/PolicyAgent"), PowerDevil::PolicyAgent::instance());
}

int main(int argc, char **argv)
{
    QGuiApplication::setDesktopSettingsAware(false);
    QGuiApplication::setAttribute(Qt::AA_DisableSessionManager);
    KWorkSpace::detectPlatform(argc, argv);
    PowerDevilApp app(argc, argv);

    bool replace = false;
    {
        QCommandLineParser parser;
        QCommandLineOption replaceOption({QStringLiteral("replace")}, i18n("Replace an existing instance"));

        parser.addOption(replaceOption);

        KAboutData aboutData = KAboutData::applicationData();
        aboutData.setupCommandLine(&parser);

        parser.process(app);
        aboutData.processCommandLine(&parser);

        replace = parser.isSet(replaceOption);
    }
    KDBusService service(KDBusService::Unique | KDBusService::StartupOption(replace ? KDBusService::Replace : 0));
    KCrash::setFlags(KCrash::AutoRestart);

    app.setQuitOnLastWindowClosed(false);
    app.init();

    return app.exec();
}
