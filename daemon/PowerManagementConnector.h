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
 ***************************************************************************/

#ifndef POWERMANAGEMENTCONNECTOR_H
#define POWERMANAGEMENTCONNECTOR_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QMultiMap>

#include <QtDBus/QDBusMessage>

#include "PowerDevilDaemon.h"

class PowerManagementConnector : public QObject
{
    Q_OBJECT

public:
    PowerManagementConnector(PowerDevilDaemon *parent);

    bool CanHibernate();
    bool CanSuspend();

    bool GetPowerSaveStatus();

    void Suspend();
    void Hibernate();

    bool HasInhibit();

    int Inhibit(const QString &application, const QString &reason);
    void UnInhibit(int cookie);

signals:
    void CanSuspendChanged(bool canSuspend);
    void CanHibernateChanged(bool canHibernate);
    void PowerSaveStatusChanged(bool savePower);

    void HasInhibitChanged(bool hasInhibit);

private slots:
    void _k_stateChanged(int battery, bool plugged);

private:
    PowerDevilDaemon *m_daemon;


};

#endif /*POWERMANAGEMENTCONNECTOR_H*/
