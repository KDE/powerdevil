/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

#include <optional>

namespace PowerDevil::BundledActions
{
/**
 * Sets the IdleHint of the user's graphical session in logind.
 *
 * PowerDevil runs as a systemd user service, outside the scope of the session, so the session
 * is the one that the Display property of the logind user object names.
 */
class LogindIdleHintClient : public QObject
{
    Q_OBJECT
public:
    explicit LogindIdleHintClient(const QDBusConnection &bus, const QString &service, QObject *parent = nullptr);

    /** Sends @p idle to logind, once the session is known and only when it changed. */
    void setIdle(bool idle);

private:
    void send();

    QDBusConnection m_bus;
    QString m_service;
    QString m_sessionPath;
    bool m_idle = false;
    std::optional<bool> m_sentIdle;
};
}
