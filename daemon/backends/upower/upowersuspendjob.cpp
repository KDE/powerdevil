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

#include "upowersuspendjob.h"

#include "upower_interface.h"

#include <powerdevil_debug.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QTimer>
#include <QDebug>
#include <KLocalizedString>

UPowerSuspendJob::UPowerSuspendJob(OrgFreedesktopUPowerInterface *upowerInterface,
                                   PowerDevil::BackendInterface::SuspendMethod method,
                                   PowerDevil::BackendInterface::SuspendMethods supported)
    : KJob(), m_upowerInterface(upowerInterface)
{
    qCDebug(POWERDEVIL) << "Starting UPower suspend job";
    m_method = method;
    m_supported = supported;

    connect(m_upowerInterface, SIGNAL(Resuming()), this, SLOT(resumeDone()));
}

UPowerSuspendJob::~UPowerSuspendJob()
{

}

void UPowerSuspendJob::start()
{
    QTimer::singleShot(0, this, SLOT(doStart()));
}

void UPowerSuspendJob::doStart()
{
    if (m_supported & m_method)
    {
        switch(m_method)
        {
        case PowerDevil::BackendInterface::ToRam:
            m_upowerInterface->AboutToSleep("suspend");
            m_upowerInterface->Suspend();
            break;
        case PowerDevil::BackendInterface::ToDisk:
            m_upowerInterface->AboutToSleep("hibernate");
            m_upowerInterface->Hibernate();
            break;
        default:
            qCDebug(POWERDEVIL) << "This backend doesn't support hybrid mode";
            setError(1);
            setErrorText(i18n("Unsupported suspend method"));
            break;
        }
        emitResult();
    }
}


void UPowerSuspendJob::resumeDone()
{
    emitResult();
}
