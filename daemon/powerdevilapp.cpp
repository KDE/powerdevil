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

#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <QFileInfo>
#include <QAction>
#include <QKeySequence>

#include <KCrash>
#include <KDBusService>
#include <KAboutData>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KGlobalAccel>

PowerDevilApp::PowerDevilApp(int &argc, char **argv)
    : QGuiApplication(argc, argv)
    , m_core(nullptr)
{
    migratePre512KeyboardShortcuts();
}

PowerDevilApp::~PowerDevilApp()
{
    delete m_core;
}

void PowerDevilApp::init()
{
//     KGlobal::insertCatalog("powerdevil");

//     KAboutData aboutData("powerdevil", "powerdevil", ki18n("KDE Power Management System"),
//                          PROJECT_VERSION, ki18n("KDE Power Management System is PowerDevil, an "
//                                                    "advanced, modular and lightweight Power Management "
//                                                    "daemon"),
//                          KAboutData::License_GPL, ki18n("(c) 2010 MetalWorkers Co."),
//                          KLocalizedString(), "http://www.kde.org");
//
//     aboutData.addAuthor(ki18n( "Dario Freddi" ), ki18n("Maintainer"), "drf@kde.org",
//                         "http://drfav.wordpress.com");

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("org.freedesktop.PowerManagement")) ||
        QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("com.novell.powersave")) ||
        QDBusConnection::systemBus().interface()->isServiceRegistered(QLatin1String("org.freedesktop.Policy.Power"))) {
        qCCritical(POWERDEVIL) << "KDE Power Management system not initialized, another power manager has been detected";
        return;
    }

    // not parenting Core to PowerDevilApp as it is the deleted too late on teardown
    // where the X connection is already lost leading to a a crash (Bug 371127)
    m_core = new PowerDevil::Core(nullptr/*, KComponentData(aboutData)*/);

    connect(m_core, SIGNAL(coreReady()), this, SLOT(onCoreReady()));

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
    Q_FOREACH (const QFileInfo &f, fileInfos) {
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


/*
 * 5.11 -> 5.12 migrated shortcuts from kded5 to the correct component name org_kde_powerdevil for good reasons
however despite a kconfupdate script working correctly and moving the old keys,
because kglobalaccel is running whilst we update it synced the old values, and then ignores the powerdevil copy
on future loads

this removes the old powermanagent entries in the kded5 component at powerdevil startup
//which is at runtime where we can talk to kglobalaccel and then re-register ours

this method can probably be deleted at some point in the future
*/
void PowerDevilApp::migratePre512KeyboardShortcuts()
{
    auto configGroup = KSharedConfig::openConfig("powermanagementprofilesrc")->group("migration");
    if (!configGroup.hasKey("kdedShortcutMigration")) {
        const QStringList actionIds({
            "Decrease Keyboard Brightness",
            "Decrease Screen Brightness",
            "Hibernate",
            "Increase Keyboard Brightness",
            "Increase Screen Brightness",
            "PowerOff",
            "Sleep",
            "Toggle Keyboard Backlight"
        });
        for (const QString &actionId: actionIds) {
            QAction oldAction;
            oldAction.setObjectName(actionId);
            oldAction.setProperty("componentName", "kded5");

            //claim the old shortcut so we can remove it..
            KGlobalAccel::self()->setShortcut(&oldAction, QList<QKeySequence>(), KGlobalAccel::Autoloading);
            auto shortcuts = KGlobalAccel::self()->shortcut(&oldAction);
            KGlobalAccel::self()->removeAllShortcuts(&oldAction);

            QAction newAction;
            newAction.setObjectName(actionId);
            newAction.setProperty("componentName", "org_kde_powerdevil");
            if (!shortcuts.isEmpty()) {
                //register with no autoloading to sync config, we then delete our QAction, and powerdevil will
                //re-register as normal
                KGlobalAccel::self()->setShortcut(&newAction, shortcuts, KGlobalAccel::NoAutoloading);
            }
        }
    }
    configGroup.writeEntry(QStringLiteral("kdedShortcutMigration"), true);
    configGroup.sync();
}

int main(int argc, char **argv)
{
    QGuiApplication::setDesktopSettingsAware(false);
    PowerDevilApp app(argc, argv);

    KDBusService service(KDBusService::Unique);
    KCrash::setFlags(KCrash::AutoRestart);

    app.setQuitOnLastWindowClosed(false);
    app.init();

    return app.exec();
}
