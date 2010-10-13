/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>

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

#ifndef UPOWERSUSPENDJOB_H
#define UPOWERSUSPENDJOB_H

#include <kjob.h>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>

#include "powerdevilupowerbackend.h"

class OrgFreedesktopUPowerInterface;

class UPowerSuspendJob : public KJob
{
    Q_OBJECT
public:
    UPowerSuspendJob(OrgFreedesktopUPowerInterface *upowerInterface,
                  PowerDevil::BackendInterface::SuspendMethod method,
                  PowerDevil::BackendInterface::SuspendMethods supported);
    virtual ~UPowerSuspendJob();

    void start();
    void kill(bool quietly);

private Q_SLOTS:
    void doStart();
    void resumeDone(const QDBusMessage &reply);

private:
    OrgFreedesktopUPowerInterface *m_upowerInterface;
    PowerDevil::BackendInterface::SuspendMethod m_method;
    PowerDevil::BackendInterface::SuspendMethods m_supported;
};

#endif //UPOWERSUSPENDJOB_H
