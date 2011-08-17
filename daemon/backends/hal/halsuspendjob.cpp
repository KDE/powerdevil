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

#include "halsuspendjob.h"

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QTimer>
#include <KConfig>
#include <KConfigGroup>

HalSuspendJob::HalSuspendJob(QDBusInterface &powermanagement, QDBusInterface &computer,
                             PowerDevil::BackendInterface::SuspendMethod method,
                             PowerDevil::BackendInterface::SuspendMethods supported)
    : KJob(), m_halPowerManagement(powermanagement), m_halComputer(computer)
{
    if (supported  & method)
    {
        QDBusReply<bool> reply;

        switch(method)
        {
        case PowerDevil::BackendInterface::ToRam:
            reply = m_halComputer.call("GetPropertyBoolean", "power_management.can_suspend_hybrid");

            if (reply.isValid())
            {
                bool can_hybrid = reply;
                if (can_hybrid)
                {
                    // Temporary: let's check if system is configured to use Hybrid suspend. Default is no.
                    KConfig sconf("suspendpreferencesrc");
                    KConfigGroup group(&sconf, "Preferences");
                    if (group.readEntry("use_hybrid", false)) {
                        m_dbusMethod = "SuspendHybrid";
                    } else {
                        m_dbusMethod = "Suspend";
                    }
                }
                else
                {
                    m_dbusMethod = "Suspend";
                }
            }
            else
            {
                m_dbusMethod = "Suspend";
            }
            m_dbusParam = 0;
            break;
        case PowerDevil::BackendInterface::ToDisk:
            m_dbusMethod = "Hibernate";
            m_dbusParam = -1;
            break;
        default:
            break;
        }
    }
}

HalSuspendJob::~HalSuspendJob()
{

}

void HalSuspendJob::start()
{
    QTimer::singleShot(0, this, SLOT(doStart()));
}

void HalSuspendJob::kill(bool /*quietly */)
{

}

void HalSuspendJob::doStart()
{
    if (m_dbusMethod.isEmpty())
    {
        setError(1);
        setErrorText("Unsupported suspend method");
        emitResult();
        return;
    }

    QList<QVariant> args;

    if (m_dbusParam>=0)
    {
        args << m_dbusParam;
    }

    if (!m_halPowerManagement.callWithCallback(m_dbusMethod, args,
                                                this, SLOT(resumeDone(QDBusMessage))))
    {
        setError(1);
        setErrorText(m_halPowerManagement.lastError().name()+": "
                     +m_halPowerManagement.lastError().message());
        emitResult();
    }
}


void HalSuspendJob::resumeDone(const QDBusMessage &reply)
{
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        // We ignore the NoReply error, since we can timeout anyway
        // if the computer is suspended for too long.
        if (reply.errorName() != "org.freedesktop.DBus.Error.NoReply")
        {
            setError(1);
            setErrorText(reply.errorName()+": "+reply.arguments().at(0).toString());
        }
    }

    emitResult();
}

#include "halsuspendjob.moc"
