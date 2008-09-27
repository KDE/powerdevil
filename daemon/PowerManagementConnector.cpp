/***************************************************************************
 *   Copyright (C) 2008 by Kevin Ottens <ervin@kde.org>                    *
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

#include <solid/control/powermanager.h>

#include "powermanagementadaptor.h"
#include "powermanagementinhibitadaptor.h"

PowerManagementConnector::PowerManagementConnector(PowerDevilDaemon *parent)
        : QObject(parent), m_daemon(parent), m_latestInhibitCookie(0)
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
    if (!HasInhibit()) {
        m_daemon->suspend(PowerDevilDaemon::S2Ram);
    }
    //TODO: Notify the user in case of inhibit?
}

void PowerManagementConnector::Hibernate()
{
    if (!HasInhibit()) {
        m_daemon->suspend(PowerDevilDaemon::S2Disk);
    }
    //TODO: Notify the user in case of inhibit?
}

bool PowerManagementConnector::HasInhibit()
{
    return !m_inhibitRequests.isEmpty();
}

int PowerManagementConnector::Inhibit(const QString &application, const QString &reason)
{
    m_latestInhibitCookie++;

    InhibitRequest req;
    //TODO: Keep track of the service name too, to cleanup cookie in case of a crash.
    req.application = application;
    req.reason = reason;
    m_inhibitRequests[m_latestInhibitCookie] = req;

    return m_latestInhibitCookie;
}

void PowerManagementConnector::UnInhibit(int cookie)
{
    m_inhibitRequests.remove(cookie);
}

void PowerManagementConnector::_k_stateChanged(int battery, bool plugged)
{
    emit PowerSaveStatusChanged(!plugged);
}

#include "PowerManagementConnector.moc"
