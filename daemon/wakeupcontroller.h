/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2020 Bhushan Shah <bhush94@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QObject>

class QSocketNotifier;

struct WakeupInfo {
    QString service;
    QDBusObjectPath path;
    int cookie;
    qint64 timeout;
};

class WakeupController : public QObject
{
    Q_OBJECT

public:
    WakeupController();

    struct ScheduleResult {
        uint cookie;
        QDBusError error;
    };
    [[nodiscard]] ScheduleResult scheduleWakeup(const QString &service, const QDBusObjectPath &path, qint64 timeout);
    [[nodiscard]] QDBusError clearWakeup(int cookie);

    void resetAndScheduleNextWakeup();
    void timerfdEventHandler();

private:
    QList<WakeupInfo> m_scheduledWakeups;
    int m_lastWakeupCookie = 0;
    int m_timerFd = -1;
    QSocketNotifier *m_timerFdSocketNotifier = nullptr;
};
