/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "suspendcontroller.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "login1suspendjob.h"

#define LOGIN1_SERVICE "org.freedesktop.login1"
#define CONSOLEKIT2_SERVICE "org.freedesktop.ConsoleKit"

SuspendController::SuspendController()
    : QObject()
{
    // interfaces
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(LOGIN1_SERVICE);
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(CONSOLEKIT2_SERVICE);
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        m_login1Interface = new QDBusInterface(LOGIN1_SERVICE, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus(), this);
    }

    // if login1 isn't available, try using the same interface with ConsoleKit2
    if (!m_login1Interface && QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        m_login1Interface = new QDBusInterface(CONSOLEKIT2_SERVICE,
                                               "/org/freedesktop/ConsoleKit/Manager",
                                               "org.freedesktop.ConsoleKit.Manager",
                                               QDBusConnection::systemBus(),
                                               this);
    }

    // Supported suspend methods
    if (m_login1Interface) {
        QDBusPendingReply<QString> canSuspend = m_login1Interface.data()->asyncCall("CanSuspend");
        QDBusPendingReply<QString> canHibernate = m_login1Interface.data()->asyncCall("CanHibernate");
        QDBusPendingReply<QString> canHybridSleep = m_login1Interface.data()->asyncCall("CanHybridSleep");
        QDBusPendingReply<QString> canSuspendThenHibernate = m_login1Interface.data()->asyncCall("CanSuspendThenHibernate");

        canSuspend.waitForFinished();
        canHibernate.waitForFinished();
        canHybridSleep.waitForFinished();
        canSuspendThenHibernate.waitForFinished();
        if (canSuspend.isValid() && (canSuspend.value() == QLatin1String("yes") || canSuspend.value() == QLatin1String("challenge"))) {
            m_suspendMethods |= ToRam;
        }
        if (canHibernate.isValid() && (canHibernate.value() == QLatin1String("yes") || canHibernate.value() == QLatin1String("challenge"))) {
            m_suspendMethods |= ToDisk;
        }
        if (canHybridSleep.isValid() && (canHybridSleep.value() == QLatin1String("yes") || canHybridSleep.value() == QLatin1String("challenge"))) {
            m_suspendMethods |= HybridSuspend;
        }
        if (canSuspendThenHibernate.isValid()
            && (canSuspendThenHibernate.value() == QLatin1String("yes") || canSuspendThenHibernate.value() == QLatin1String("challenge"))) {
            m_suspendMethods |= SuspendThenHibernate;
        }
    }

    // "resuming" signal
    if (m_login1Interface) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1PrepareForSleep(bool)));
    }
}

KJob *SuspendController::suspend(SuspendMethod method)
{
    if (m_login1Interface) {
        return new Login1SuspendJob(m_login1Interface.data(), method, supportedSuspendMethods());
    }
    return nullptr;
}

SuspendController::SuspendMethods SuspendController::supportedSuspendMethods() const
{
    return m_suspendMethods;
}

void SuspendController::slotLogin1PrepareForSleep(bool active)
{
    if (active) {
        Q_EMIT aboutToSuspend();
    } else {
        Q_EMIT resumeFromSuspend();
    }
}
