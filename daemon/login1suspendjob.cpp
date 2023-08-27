/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2013 Lukáš Tinkl <ltinkl@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "login1suspendjob.h"

#include <powerdevil_debug.h>

#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QTimer>

#include <KLocalizedString>

Login1SuspendJob::Login1SuspendJob(QDBusInterface *login1Interface, SuspendController::SuspendMethod method)
    : KJob()
    , m_login1Interface(login1Interface)
{
    qCDebug(POWERDEVIL) << "Starting Login1 suspend job";
    m_method = method;

    connect(m_login1Interface, SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1Resuming(bool)));
}

Login1SuspendJob::~Login1SuspendJob()
{
}

void Login1SuspendJob::start()
{
    QTimer::singleShot(0, this, &Login1SuspendJob::doStart);
}

void Login1SuspendJob::doStart()
{
    QVariantList args;
    args << true; // interactive, ie. with polkit dialogs

    QDBusPendingReply<void> reply;

    switch (m_method) {
    case SuspendController::ToRam:
        reply = m_login1Interface->asyncCallWithArgumentList(QStringLiteral("Suspend"), args);
        break;
    case SuspendController::ToDisk:
        reply = m_login1Interface->asyncCallWithArgumentList(QStringLiteral("Hibernate"), args);
        break;
    case SuspendController::HybridSuspend:
        reply = m_login1Interface->asyncCallWithArgumentList(QStringLiteral("HybridSleep"), args);
        break;
    case SuspendController::SuspendThenHibernate:
        reply = m_login1Interface->asyncCallWithArgumentList(QStringLiteral("SuspendThenHibernate"), args);
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

#include "moc_login1suspendjob.cpp"
