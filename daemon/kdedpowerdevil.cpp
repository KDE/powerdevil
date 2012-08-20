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

#include "powerdevilbackendloader.h"
#include "powerdevilcore.h"

#include <QtCore/QTimer>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

#include <KAboutData>
#include <KDebug>
#include <KPluginFactory>

K_PLUGIN_FACTORY( PowerDevilFactory,
                  registerPlugin<KDEDPowerDevil>(); )
K_EXPORT_PLUGIN( PowerDevilFactory( "powerdevildaemon" ) )

#define POWERDEVIL_VERSION "1.99"

KDEDPowerDevil::KDEDPowerDevil(QObject* parent, const QVariantList &)
    : KDEDModule(parent)
{
    QTimer::singleShot(0, this, SLOT(init()));
}

KDEDPowerDevil::~KDEDPowerDevil()
{
}

void KDEDPowerDevil::init()
{
    KGlobal::insertCatalog("powerdevil");

    KAboutData aboutData("powerdevil", "powerdevil", ki18n("KDE Power Management System"),
                         POWERDEVIL_VERSION, ki18n("KDE Power Management System is PowerDevil, an "
                                                   "advanced, modular and lightweight Power Management "
                                                   "daemon"),
                         KAboutData::License_GPL, ki18n("(c) 2010 MetalWorkers Co."),
                         KLocalizedString(), "http://www.kde.org");

    aboutData.addAuthor(ki18n( "Dario Freddi" ), ki18n("Maintainer"), "drf@kde.org",
                        "http://drfav.wordpress.com");

    if (QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.PowerManagement") ||
        QDBusConnection::systemBus().interface()->isServiceRegistered("com.novell.powersave") ||
        QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.Policy.Power")) {
        kError() << "KDE Power Management system not initialized, another power manager has been detected";
        return;
    }

    m_core = new PowerDevil::Core(this, KComponentData(aboutData));

    connect(m_core, SIGNAL(coreReady()), this, SLOT(onCoreReady()));

    // Before doing anything, let's set up our backend
    PowerDevil::BackendInterface *interface = PowerDevil::BackendLoader::loadBackend(m_core);

    if (!interface) {
        // Ouch
        kError() << "KDE Power Management System init failed!";
        m_core->loadCore(0);
    } else {
        // Let's go!
        kDebug() << "Backend loaded, loading core";
        m_core->loadCore(interface);
    }
}

void KDEDPowerDevil::onCoreReady()
{
    kDebug() << "Core is ready, registering various services on the bus...";
    //DBus logic for the core
    new PowerManagementAdaptor(m_core);
    new PowerDevil::FdoConnector(m_core);

    QDBusConnection::sessionBus().registerService("org.kde.Solid.PowerManagement");
    QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement", m_core);

    QDBusConnection::systemBus().interface()->registerService("org.freedesktop.Policy.Power");

    // Start the Policy Agent service
    new PowerManagementPolicyAgentAdaptor(PowerDevil::PolicyAgent::instance());

    QDBusConnection::sessionBus().registerService("org.kde.Solid.PowerManagement.PolicyAgent");
    QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement/PolicyAgent", PowerDevil::PolicyAgent::instance());
}

#include "kdedpowerdevil.moc"
