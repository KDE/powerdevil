/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2013 Lukáš Tinkl <ltinkl@redhat.com>

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

#include "login1suspendjob.h"

#include <powerdevil_debug.h>

#include <QDBusReply>
#include <QDBusMessage>
#include <QTimer>
#include <QDebug>

#include <KLocalizedString>

Login1SuspendJob::Login1SuspendJob(QDBusInterface *login1Interface,
                                   PowerDevil::BackendInterface::SuspendMethod method,
                                   PowerDevil::BackendInterface::SuspendMethods supported)
    : KJob(), m_login1Interface(login1Interface)
{
    qCDebug(POWERDEVIL) << "Starting Login1 suspend job";
    m_method = method;
    m_supported = supported;

    connect(m_login1Interface, SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1Resuming(bool)));
}

Login1SuspendJob::~Login1SuspendJob()
{

}

void Login1SuspendJob::start()
{
    QTimer::singleShot(0, this, SLOT(doStart()));
}

void Login1SuspendJob::doStart()
{
    if (m_supported & m_method) {
        QVariantList args;
        args << true; // interactive, ie. with polkit dialogs

        QDBusPendingReply<void> reply;

        switch(m_method) {
        case PowerDevil::BackendInterface::ToRam:
            reply = m_login1Interface->asyncCallWithArgumentList("Suspend", args);
            break;
        case PowerDevil::BackendInterface::ToDisk:
            reply = m_login1Interface->asyncCallWithArgumentList("Hibernate", args);
            break;
        case PowerDevil::BackendInterface::HybridSuspend:
            reply = m_login1Interface->asyncCallWithArgumentList("HybridSleep", args);
            break;
        default:
            qCDebug(POWERDEVIL) << "Unsupported suspend method";
            setError(1);
            setErrorText(i18n("Unsupported suspend method"));
            return;
        }

        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &Login1SuspendJob::sendResult);
    }
}

void Login1SuspendJob::sendResult(QDBusPendingCallWatcher *watcher)
{
    const QDBusPendingReply<void> reply = *watcher;
    watcher->deleteLater();
    if (!reply.isError()) {
        emitResult();
    } else {
        qCWarning(POWERDEVIL) << "Failed to start suspend job" << reply.error().name() << reply.error().message();
    }
}

void Login1SuspendJob::slotLogin1Resuming(bool active)
{
    if (!active)
        emitResult();
}
