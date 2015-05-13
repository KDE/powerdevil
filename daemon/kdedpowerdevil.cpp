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
#include "powerdevil_debug.h"

#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>

#include <KAboutData>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY( PowerDevilFactory,
                  registerPlugin<KDEDPowerDevil>(); )

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
    if (QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.PowerManagement") ||
        QDBusConnection::systemBus().interface()->isServiceRegistered("com.novell.powersave") ||
        QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.Policy.Power")) {
        qCCritical(POWERDEVIL) << "KDE Power Management system not initialized, another power manager has been detected";
        return;
    }

    m_core = new PowerDevil::Core(this);

    connect(m_core, SIGNAL(coreReady()), this, SLOT(onCoreReady()));

    // Before doing anything, let's set up our backend
    PowerDevil::BackendInterface *interface = PowerDevil::BackendLoader::loadBackend(m_core);

    if (!interface) {
        // Ouch
        qCCritical(POWERDEVIL) << "KDE Power Management System init failed!";
        m_core->loadCore(0);
    } else {
        // Let's go!
        qCDebug(POWERDEVIL) << "Backend loaded, loading core";
        m_core->loadCore(interface);
    }
}

void KDEDPowerDevil::onCoreReady()
{
    qCDebug(POWERDEVIL) << "Core is ready, registering various services on the bus...";
    //DBus logic for the core
    new PowerManagementAdaptor(m_core);
    new PowerDevil::FdoConnector(m_core);

    QDBusConnection::sessionBus().registerService("org.kde.Solid.PowerManagement");
    QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement", m_core);

    QDBusConnection::systemBus().interface()->registerService("org.freedesktop.Policy.Power");

    // Start the Policy Agent service
    qDBusRegisterMetaType<QList<InhibitionInfo>>();
    qDBusRegisterMetaType<InhibitionInfo>();
    new PowerManagementPolicyAgentAdaptor(PowerDevil::PolicyAgent::instance());

    QDBusConnection::sessionBus().registerService("org.kde.Solid.PowerManagement.PolicyAgent");
    QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement/PolicyAgent", PowerDevil::PolicyAgent::instance());
}

#include "kdedpowerdevil.moc"
