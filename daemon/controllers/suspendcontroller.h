/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include <QDBusInterface>
#include <QObject>
#include <QPointer>

#include <KJob>

#include <sessionmanagement.h>

#include "powerdevilcore_export.h"
#include "udevqtclient.h"

class POWERDEVILCORE_EXPORT SuspendController : public QObject
{
    Q_OBJECT
public:
    SuspendController();

    enum class WakeupSource {
        UnknownSource = 0,
        Timer = 1,
        PowerManagement = 2,
        Network = 4,
        Telephony = 8,
        AC = 16,
    };
    Q_ENUM(WakeupSource)

    /**
     * This type stores an OR combination of WakeupSource values
     */
    Q_DECLARE_FLAGS(WakeupSources, WakeupSource)

    bool canSuspend() const;
    void suspend();

    bool canHibernate() const;
    void hibernate();

    bool canHybridSuspend() const;
    void hybridSuspend();

    bool canSuspendThenHibernate() const;
    void suspendThenHibernate();

    WakeupSources lastWakeupType();
    QStringList wakeupDevices();

Q_SIGNALS:
    /**
     * This signal is emitted when the PC is resuming from suspension
     */
    void resumeFromSuspend();

    /**
     * This signal is emitted when the PC is about to suspend
     */
    void aboutToSuspend();

private Q_SLOTS:
    void slotLogin1PrepareForSleep(bool active);

private:
    SessionManagement m_sessionManagement;

    // login1 interface
    QPointer<QDBusInterface> m_login1Interface;

#ifdef Q_OS_LINUX
    void snapshotWakeupCounts(bool active);
#endif
    QHash<QString, int> m_wakeupCounts;
    QStringList m_lastWakeupSources;
    UdevQt::Client *m_udevClient;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SuspendController::WakeupSources)
