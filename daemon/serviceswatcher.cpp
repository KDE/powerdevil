/*
 *   SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "serviceswatcher.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>

namespace PowerDevil
{

ServicesWatcher::ServicesWatcher(const QString &action, const QStringList &watchedServices, QObject *parent)
    : QObject(parent)
    , m_action(action)
{
    for (const QString &service : watchedServices) {
        if (service.isEmpty()) {
            continue;
        }
        ++m_total;
        auto watcher = new QDBusServiceWatcher(service,
                                               QDBusConnection::sessionBus(),
                                               QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                               this);
        connect(watcher, &QDBusServiceWatcher::serviceRegistered, [this] {
            Q_ASSERT_X(m_registered < m_total, Q_FUNC_INFO, qPrintable(m_action));
            if (++m_registered == m_total) {
                Q_EMIT allServicesOnline();
            }
        });
        connect(watcher, &QDBusServiceWatcher::serviceUnregistered, [this] {
            Q_ASSERT_X(m_registered > 0, Q_FUNC_INFO, qPrintable(m_action));
            m_registered -= 1;
        });
    }

    if (m_total == 0) [[unlikely]] {
        Q_ASSERT_X(false, Q_FUNC_INFO, qPrintable(m_action));
        QMetaObject::invokeMethod(
            this,
            [this] {
                Q_EMIT allServicesOnline();
            },
            Qt::QueuedConnection);
    }
}

QString ServicesWatcher::action() const
{
    return m_action;
}
}