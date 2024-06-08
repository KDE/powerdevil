/*
 *   SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

class QDBusServiceWatcher;

namespace PowerDevil
{
class ServicesWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ServicesWatcher(const QString &action, const QStringList &watchedServices, QObject *parent);
    QString action() const;

Q_SIGNALS:
    void allServicesOnline();

private:
    QString m_action;
    unsigned m_total = 0;
    unsigned m_registered = 0;
};
}