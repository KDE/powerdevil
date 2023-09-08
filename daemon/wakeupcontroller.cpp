/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2020 Bhushan Shah <bhush94@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wakeupcontroller.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDateTime>
#include <QSocketNotifier>

#ifdef Q_OS_LINUX
#include <sys/timerfd.h>
#endif

#include "powerdevil_debug.h"

WakeupController::WakeupController()
    : QObject()
{
#ifdef Q_OS_LINUX

    // try creating a timerfd which can wake system from suspend
    m_timerFd = timerfd_create(CLOCK_REALTIME_ALARM, TFD_CLOEXEC);

    // if that fails due to privilges maybe, try normal timerfd
    if (m_timerFd == -1) {
        qCDebug(POWERDEVIL)
            << "Unable to create a CLOCK_REALTIME_ALARM timer, trying CLOCK_REALTIME\n This would mean that wakeup requests won't wake device from suspend";
        m_timerFd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
    }

    if (m_timerFd != -1) {
        m_timerFdSocketNotifier = new QSocketNotifier(m_timerFd, QSocketNotifier::Read, this);
        connect(m_timerFdSocketNotifier, &QSocketNotifier::activated, this, &WakeupController::timerfdEventHandler);
        // we disable events reading for now
        m_timerFdSocketNotifier->setEnabled(false);
    } else {
        qCDebug(POWERDEVIL) << "Unable to create a CLOCK_REALTIME timer, scheduled wakeups won't be available";
    }

#endif
}

WakeupController::ScheduleResult WakeupController::scheduleWakeup(const QString &service, const QDBusObjectPath &path, qint64 timeout)
{
    ++m_lastWakeupCookie;

    int cookie = m_lastWakeupCookie;
    // if some one is trying to time travel, deny them
    if (timeout < QDateTime::currentSecsSinceEpoch()) {
        return ScheduleResult{0, QDBusError(QDBusError::InvalidArgs, "You can not schedule wakeup in past")};
    } else {
#ifndef Q_OS_LINUX
        return ScheduleResult{0, QDBusError(QDBusError::NotSupported, "Scheduled wakeups are available only on Linux platforms")};
#else
        WakeupInfo wakeup{service, path, cookie, timeout};
        m_scheduledWakeups << wakeup;
        qCDebug(POWERDEVIL) << "Received request to wakeup at " << QDateTime::fromSecsSinceEpoch(timeout);
        resetAndScheduleNextWakeup();
#endif
    }
    return ScheduleResult{static_cast<uint>(cookie), QDBusError(QDBusError::NoError, QString())};
}

QDBusError WakeupController::clearWakeup(int cookie)
{
    // if we do not have any timeouts return from here
    if (m_scheduledWakeups.isEmpty()) {
        return QDBusError(QDBusError::NoError, QString());
    }

    int oldListSize = m_scheduledWakeups.size();

    // depending on cookie, remove it from scheduled wakeups
    m_scheduledWakeups.erase(std::remove_if(m_scheduledWakeups.begin(),
                                            m_scheduledWakeups.end(),
                                            [cookie](WakeupInfo wakeup) {
                                                return wakeup.cookie == cookie;
                                            }),
                             m_scheduledWakeups.end());

    if (oldListSize == m_scheduledWakeups.size()) {
        return QDBusError(QDBusError::InvalidArgs, "Can not clear the invalid wakeup");
    }

    // reset timerfd
    resetAndScheduleNextWakeup();

    return QDBusError(QDBusError::NoError, QString());
}

void WakeupController::resetAndScheduleNextWakeup()
{
#ifdef Q_OS_LINUX
    // first we sort the wakeup list
    std::sort(m_scheduledWakeups.begin(), m_scheduledWakeups.end(), [](const WakeupInfo &lhs, const WakeupInfo &rhs) {
        return lhs.timeout < rhs.timeout;
    });

    // we don't want any of our wakeups to repeat'
    timespec interval = {0, 0};
    timespec nextWakeup;
    bool enableNotifier = false;
    // if we don't have any wakeups left, we call it a day and stop timer_fd
    if (m_scheduledWakeups.isEmpty()) {
        nextWakeup = {0, 0};
    } else {
        // now pick the first timeout from the list
        WakeupInfo wakeup = m_scheduledWakeups.first();
        nextWakeup = {wakeup.timeout, 0};
        enableNotifier = true;
    }
    if (m_timerFd != -1) {
        const itimerspec spec = {interval, nextWakeup};
        timerfd_settime(m_timerFd, TFD_TIMER_ABSTIME, &spec, nullptr);
    }
    m_timerFdSocketNotifier->setEnabled(enableNotifier);
#endif
}

void WakeupController::timerfdEventHandler()
{
    // wakeup from the linux/rtc

    // Disable reading events from the timer_fd
    m_timerFdSocketNotifier->setEnabled(false);

    // At this point scheduled wakeup list should not be empty, but just in case
    if (m_scheduledWakeups.isEmpty()) {
        qWarning(POWERDEVIL) << "Wakeup was recieved but list is now empty! This should not happen!";
        return;
    }

    // first thing to do is, we pick the first wakeup from list
    WakeupInfo currentWakeup = m_scheduledWakeups.takeFirst();

    // Before doing anything further, lets set the next set of wakeup alarm
    resetAndScheduleNextWakeup();

    // Now current wakeup needs to be processed
    // prepare message for sending back to the consumer
    QDBusMessage msg = QDBusMessage::createMethodCall(currentWakeup.service,
                                                      currentWakeup.path.path(),
                                                      QStringLiteral("org.kde.PowerManagement"),
                                                      QStringLiteral("wakeupCallback"));
    msg << currentWakeup.cookie;
    // send it away
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}
