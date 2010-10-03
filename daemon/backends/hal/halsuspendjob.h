/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef HALSUSPENDJOB_H
#define HALSUSPENDJOB_H

#include <kjob.h>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>

#include "powerdevilhalbackend.h"

class HalSuspendJob : public KJob
{
    Q_OBJECT
public:
    HalSuspendJob(QDBusInterface &powermanagement, QDBusInterface &computer,
                  PowerDevil::BackendInterface::SuspendMethod method,
                  PowerDevil::BackendInterface::SuspendMethods supported);
    virtual ~HalSuspendJob();

    void start();
    void kill(bool quietly);

private Q_SLOTS:
    void doStart();
    void resumeDone(const QDBusMessage &reply);

private:
    QDBusInterface &m_halPowerManagement;
    QDBusInterface &m_halComputer;
    QString m_dbusMethod;
    int m_dbusParam;
};

#endif
