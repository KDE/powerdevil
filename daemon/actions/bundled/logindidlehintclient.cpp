/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "logindidlehintclient.h"

#include <powerdevil_debug.h>

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

using namespace Qt::Literals::StringLiterals;

namespace PowerDevil::BundledActions
{
LogindIdleHintClient::LogindIdleHintClient(const QDBusConnection &bus, const QString &service, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_service(service)
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, u"/org/freedesktop/login1/user/self"_s, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    message << u"org.freedesktop.login1.User"_s << u"Display"_s;
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (reply.isError()) {
            qCDebug(POWERDEVIL) << "No logind session to set the idle hint of:" << reply.error().message();
            return;
        }
        // Display is a (so) struct: the session id and its object path.
        const QDBusArgument argument = reply.value().variant().value<QDBusArgument>();
        QString sessionId;
        QDBusObjectPath sessionPath;
        argument.beginStructure();
        argument >> sessionId >> sessionPath;
        argument.endStructure();
        // A user without a graphical session has "/" here.
        if (sessionPath.path().isEmpty() || sessionPath.path() == u"/"_s) {
            qCDebug(POWERDEVIL) << "The user has no graphical logind session to set the idle hint of";
            return;
        }
        m_sessionPath = sessionPath.path();
        send();
    });
}

void LogindIdleHintClient::setIdle(bool idle)
{
    m_idle = idle;
    send();
}

void LogindIdleHintClient::send()
{
    if (m_sessionPath.isEmpty() || m_sentIdle == m_idle) {
        return;
    }
    m_sentIdle = m_idle;

    QDBusMessage message = QDBusMessage::createMethodCall(m_service, m_sessionPath, u"org.freedesktop.login1.Session"_s, u"SetIdleHint"_s);
    message << m_idle;
    auto watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) {
            qCWarning(POWERDEVIL) << "Failed to set the logind idle hint:" << reply.error().message();
            // Send again on the next change.
            m_sentIdle.reset();
        }
    });
}
}

#include "moc_logindidlehintclient.cpp"
