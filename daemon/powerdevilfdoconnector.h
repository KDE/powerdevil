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
 ***************************************************************************/

#ifndef POWERDEVILFDOCONNECTOR_H
#define POWERDEVILFDOCONNECTOR_H

#include <QObject>

#include "powerdevilbackendinterface.h"
#include "powerdevilpolicyagent.h"

namespace PowerDevil {
class Core;

class FdoConnector : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(FdoConnector)

    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PowerManagement")
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PowerManagement.Inhibit")

public:
    FdoConnector(PowerDevil::Core *parent);

    bool CanHibernate();
    bool CanSuspend();
    bool CanHybridSuspend();

    bool GetPowerSaveStatus();

    void Suspend();
    void Hibernate();
    void HybridSuspend();

    bool HasInhibit();

    int Inhibit(const QString &application, const QString &reason);
    void UnInhibit(int cookie);
    void ForceUnInhibitAll();

Q_SIGNALS:
    void CanSuspendChanged(bool canSuspend);
    void CanHibernateChanged(bool canHibernate);
    void CanHybridSuspendChanged(bool canHybridSuspend);
    void PowerSaveStatusChanged(bool savePower);

    void HasInhibitChanged(bool hasInhibit);

private Q_SLOTS:
    void onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState);
    void onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies);
    void triggerSuspendSession(uint action);

private:
    PowerDevil::Core *m_core;
};

}

#endif /*POWERDEVILFDOCONNECTOR_H*/
