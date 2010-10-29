/***************************************************************************
 *   Copyright (C) 2008 by Kevin Ottens <ervin@kde.org>                    *
 *   Copyright (C) 2008-2010 by Dario Freddi <drf@kde.org>                 *
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

#include "powerdevilcore.h"
#include "powerdevilpolicyagent.h"

#include "powermanagementfdoadaptor.h"
#include "powermanagementinhibitadaptor.h"

PowerManagementConnector::PowerManagementConnector(PowerDevil::Core *parent)
        : QObject(parent), m_core(parent)
{
    new PowerManagementFdoAdaptor(this);
    new PowerManagementInhibitAdaptor(this);

    QDBusConnection c = QDBusConnection::sessionBus();

    c.registerService("org.freedesktop.PowerManagement");
    c.registerObject("/org/freedesktop/PowerManagement", this);

    c.registerService("org.freedesktop.PowerManagement.Inhibit");
    c.registerObject("/org/freedesktop/PowerManagement/Inhibit", this);

    connect(m_core->backend(), SIGNAL(acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)),
            this, SLOT(onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)));
    connect(m_core, SIGNAL(inhibitChanged(bool)),
            this, SIGNAL(HasInhibitChanged(bool)));
}

bool PowerManagementConnector::CanHibernate()
{
    return m_core->backend()->supportedSuspendMethods() & PowerDevil::BackendInterface::ToDisk;
}

bool PowerManagementConnector::CanSuspend()
{
    return m_core->backend()->supportedSuspendMethods() & PowerDevil::BackendInterface::ToRam;
}

bool PowerManagementConnector::GetPowerSaveStatus()
{
    return m_core->backend()->acAdapterState() == PowerDevil::BackendInterface::Unplugged;
}

void PowerManagementConnector::Suspend()
{
    m_core->suspendToRam();
}

void PowerManagementConnector::Hibernate()
{
    m_core->suspendToDisk();
}

bool PowerManagementConnector::HasInhibit()
{
    return PowerDevil::PolicyAgent::instance()->requirePolicyCheck(PowerDevil::PolicyAgent::InterruptSession)
                                                                        == PowerDevil::PolicyAgent::None;
}

int PowerManagementConnector::Inhibit(const QString &application, const QString &reason)
{
    //return m_daemon->lockHandler()->inhibit(application, reason);
    return 0;
}

void PowerManagementConnector::UnInhibit(int cookie)
{
    //m_daemon->lockHandler()->releaseInhibiton(cookie);
}

void PowerManagementConnector::ForceUnInhibitAll()
{
    //m_daemon->lockHandler()->releaseAllInhibitions();
}

void PowerManagementConnector::onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState newstate)
{
    emit PowerSaveStatusChanged(newstate == PowerDevil::BackendInterface::Plugged);
}

#include "PowerManagementConnector.moc"
