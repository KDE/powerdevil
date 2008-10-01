/***************************************************************************
 *   Copyright (C) 2008 by Kevin Ottens <ervin@kde.org>                    *
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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
 **************************************************************************/

#include "PowerManagementConnector.h"

#include "SuspensionLockHandler.h"

#include <solid/control/powermanager.h>

#include "powermanagementadaptor.h"
#include "powermanagementinhibitadaptor.h"

PowerManagementConnector::PowerManagementConnector(PowerDevilDaemon *parent)
        : QObject(parent), m_daemon(parent)
{
    new PowerManagementAdaptor(this);
    new PowerManagementInhibitAdaptor(this);

    QDBusConnection c = QDBusConnection::sessionBus();

    c.registerService("org.kde.Solid.PowerManagement");
    c.registerObject("/org/kde/Solid/PowerManagement", this);

    c.registerService("org.kde.Solid.PowerManagement.Inhibit");
    c.registerObject("/org/kde/Solid/PowerManagement/Inhibit", this);

    connect(m_daemon, SIGNAL(stateChanged(int, bool)),
            this, SLOT(_k_stateChanged(int, bool)));
    connect(m_daemon->lockHandler(), SIGNAL(inhibitChanged(bool)),
            this, SIGNAL(HasInhibitChanged(bool)));
}

bool PowerManagementConnector::CanHibernate()
{
    Solid::Control::PowerManager::SuspendMethods methods
    = Solid::Control::PowerManager::supportedSuspendMethods();

    return methods & Solid::Control::PowerManager::ToDisk;
}

bool PowerManagementConnector::CanSuspend()
{
    Solid::Control::PowerManager::SuspendMethods methods
    = Solid::Control::PowerManager::supportedSuspendMethods();

    return methods & Solid::Control::PowerManager::ToRam;
}

bool PowerManagementConnector::GetPowerSaveStatus()
{
    return Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Unplugged;
}

void PowerManagementConnector::Suspend()
{
    m_daemon->suspend(PowerDevilDaemon::S2Ram);
}

void PowerManagementConnector::Hibernate()
{
    m_daemon->suspend(PowerDevilDaemon::S2Disk);
}

bool PowerManagementConnector::HasInhibit()
{
    return m_daemon->lockHandler()->hasInhibit();
}

int PowerManagementConnector::Inhibit(const QString &application, const QString &reason)
{
    return m_daemon->lockHandler()->inhibit(application, reason);
}

void PowerManagementConnector::UnInhibit(int cookie)
{
    m_daemon->lockHandler()->releaseInhibiton(cookie);
}

void PowerManagementConnector::ForceUnInhibitAll()
{
    m_daemon->lockHandler()->releaseAllInhibitions();
}

void PowerManagementConnector::_k_stateChanged(int battery, bool plugged)
{
    Q_UNUSED(battery)
    emit PowerSaveStatusChanged(!plugged);
}

#include "PowerManagementConnector.moc"
