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

#include "kdedpowerdevil.h"

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

#include <KAboutData>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY_WITH_JSON(PowerDevilFactory,
                           "powerdevil.json",
                           registerPlugin<KDEDPowerDevil>();)

KDEDPowerDevil::KDEDPowerDevil(QObject* parent, const QVariantList &)
    : KDEDModule(parent)
    , m_core(Q_NULLPTR)
{
    QTimer::singleShot(0, this, SLOT(init()));
}

KDEDPowerDevil::~KDEDPowerDevil()
{
}

void KDEDPowerDevil::init()
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

    m_core = new PowerDevil::Core(this/*, KComponentData(aboutData)*/);

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
        m_core->loadCore(0);
        return;
    }

    auto interface = qobject_cast<PowerDevil::BackendInterface*>(instance);
    if (!interface) {
        qCDebug(POWERDEVIL) << "Failed to cast plugin instance to BackendInterface, check your plugin";
        qCCritical(POWERDEVIL) << "KDE Power Management System init failed!";
        m_core->loadCore(0);
        return;
    }

    qCDebug(POWERDEVIL) << "Backend loaded, loading core";
    m_core->loadCore(interface);
}

void KDEDPowerDevil::onCoreReady()
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

#include "kdedpowerdevil.moc"
