/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/


#include "powerdevilpolicyagent.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusServiceWatcher>

#include <KGlobal>
#include <KDebug>

namespace PowerDevil
{

class PolicyAgentHelper
{
public:
    PolicyAgentHelper() : q(0) {}
    ~PolicyAgentHelper() {
        delete q;
    }
    PolicyAgent *q;
};

K_GLOBAL_STATIC(PolicyAgentHelper, s_globalPolicyAgent)

PolicyAgent *PolicyAgent::instance()
{
    if (!s_globalPolicyAgent->q) {
        new PolicyAgent;
    }

    return s_globalPolicyAgent->q;
}

PolicyAgent::PolicyAgent(QObject* parent)
    : QObject(parent)
    , m_ckAvailable(false)
    , m_sessionIsBeingInterrupted(false)
    , m_lastCookie(0)
    , m_busWatcher(new QDBusServiceWatcher(this))
    , m_ckWatcher(new QDBusServiceWatcher(this))
{
    Q_ASSERT(!s_globalPolicyAgent->q);
    s_globalPolicyAgent->q = this;
}

PolicyAgent::~PolicyAgent()
{

}

void PolicyAgent::init()
{
    // Watch over the ConsoleKit service
    m_ckWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_ckWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration |
                                     QDBusServiceWatcher::WatchForRegistration);
    m_ckWatcher.data()->addWatchedService("org.freedesktop.ConsoleKit");

    connect(m_ckWatcher.data(), SIGNAL(serviceRegistered(QString)),
            this, SLOT(onConsoleKitRegistered(QString)));
    connect(m_ckWatcher.data(), SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onConsoleKitUnregistered(QString)));
    // If it's up and running already, let's cache it
    if (QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.ConsoleKit")) {
        onConsoleKitRegistered("org.freedesktop.ConsoleKit");
    }

    // Now set up our service watcher
    m_busWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_busWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_busWatcher.data(), SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onServiceUnregistered(QString)));
}

void PowerDevil::PolicyAgent::onConsoleKitRegistered(const QString& )
{
    m_ckAvailable = true;

    // Otherwise, let's ask ConsoleKit
    QDBusInterface ckiface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                           "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());

    QDBusPendingReply<QDBusObjectPath> sessionPath = ckiface.asyncCall("GetCurrentSession");

    sessionPath.waitForFinished();

    if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
        kDebug() << "The session is not registered with ck";
        m_ckAvailable = false;
        return;
    }

    m_ckSessionInterface = new QDBusInterface("org.freedesktop.ConsoleKit", sessionPath.value().path(),
                                              "org.freedesktop.ConsoleKit.Session", QDBusConnection::systemBus());

    if (!m_ckSessionInterface.data()->isValid()) {
        // As above
        kDebug() << "Can't contact iface";
        m_ckAvailable = false;
        return;
    }

    // Now let's obtain the seat
    QDBusPendingReply< QDBusObjectPath > seatPath = m_ckSessionInterface.data()->asyncCall("GetSeatId");
    seatPath.waitForFinished();

    if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
        kDebug() << "Unable to associate ck session with a seat";
        m_ckAvailable = false;
        return;
    }

    if (!QDBusConnection::systemBus().connect("org.freedesktop.ConsoleKit", seatPath.value().path(),
                                              "org.freedesktop.ConsoleKit.Seat", "ActiveSessionChanged",
                                              this, SLOT(onConsoleKitActiveSessionChanged(QString)))) {
        kDebug() << "Unable to connect to ActiveSessionChanged";
        m_ckAvailable = false;
        return;
    }

    // Force triggering of active session changed
    QDBusMessage call = QDBusMessage::createMethodCall("org.freedesktop.ConsoleKit", seatPath.value().path(),
                                                       "org.freedesktop.ConsoleKit.Seat", "GetActiveSession");
    QDBusPendingReply< QDBusObjectPath > activeSession = QDBusConnection::systemBus().asyncCall(call);
    activeSession.waitForFinished();

    onConsoleKitActiveSessionChanged(activeSession.value().path());

    kDebug() << "ConsoleKit support initialized";
}

void PowerDevil::PolicyAgent::onConsoleKitUnregistered(const QString& )
{
    m_ckAvailable = false;
    m_ckSessionInterface.data()->deleteLater();
}

void PolicyAgent::onConsoleKitActiveSessionChanged(const QString& activeSession)
{
    if (activeSession.isEmpty()) {
        kDebug() << "Switched to inactive session - leaving unchanged";
        return;
    } else if (activeSession == m_ckSessionInterface.data()->path()) {
        kDebug() << "Current session is now active";
        m_wasLastActiveSession = true;
    } else {
        kDebug() << "Current session is now inactive";
        m_wasLastActiveSession = false;
    }
}

void PolicyAgent::onServiceUnregistered(const QString& serviceName)
{
    if (m_cookieToBusService.values().contains(serviceName)) {
        // Ouch - the application quit or crashed without releasing its inhibitions. Let's fix that.
        ReleaseInhibition(m_cookieToBusService.key(serviceName));
    }
}

PolicyAgent::RequiredPolicies PolicyAgent::unavailablePolicies()
{
    RequiredPolicies retpolicies = None;

    if (!m_typesToCookie[ChangeProfile].isEmpty()) {
        retpolicies |= ChangeProfile;
    }
    if (!m_typesToCookie[ChangeScreenSettings].isEmpty()) {
        retpolicies |= ChangeScreenSettings;
    }
    if (!m_typesToCookie[InterruptSession].isEmpty()) {
        retpolicies |= InterruptSession;
    }

    return retpolicies;
}

PolicyAgent::RequiredPolicies PolicyAgent::requirePolicyCheck(PolicyAgent::RequiredPolicies policies)
{
    if (!m_ckAvailable) {
        // No way to determine if we are on the current session, simply suppose we are
        kDebug() << "Can't contact ck";
    } else if (!m_ckSessionInterface.isNull()) {
        QDBusPendingReply< bool > rp = m_ckSessionInterface.data()->asyncCall("IsActive");
        rp.waitForFinished();

        if (!rp.value() && !m_wasLastActiveSession && policies != InterruptSession) {
            return policies;
        }
    }

    // Ok, let's go then
    RequiredPolicies retpolicies = None;

    if (policies & ChangeProfile) {
        if (!m_typesToCookie[ChangeProfile].isEmpty()) {
            retpolicies |= ChangeProfile;
        }
    }
    if (policies & ChangeScreenSettings) {
        if (!m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            retpolicies |= ChangeScreenSettings;
        }
    }
    if (policies & InterruptSession) {
        if (m_sessionIsBeingInterrupted || !m_typesToCookie[InterruptSession].isEmpty()) {
            retpolicies |= InterruptSession;
        }
    }

    return retpolicies;
}

void PolicyAgent::startSessionInterruption()
{
    m_sessionIsBeingInterrupted = true;
}

void PolicyAgent::finishSessionInterruption()
{
    m_sessionIsBeingInterrupted = false;
}

uint PolicyAgent::addInhibitionWithExplicitDBusService(uint types, const QString& appName,
                                                       const QString& reason, const QString& service)
{
    ++m_lastCookie;

    m_cookieToAppName.insert(m_lastCookie, qMakePair<QString, QString>(appName, reason));

    if (!m_busWatcher.isNull() && !service.isEmpty()) {
        m_cookieToBusService.insert(m_lastCookie, service);
        m_busWatcher.data()->addWatchedService(service);
    }

    addInhibitionTypeHelper(m_lastCookie, static_cast< PolicyAgent::RequiredPolicies >(types));

    return m_lastCookie;
}

uint PolicyAgent::AddInhibition(uint types,
                                const QString& appName,
                                const QString& reason)
{
    ++m_lastCookie;

    m_cookieToAppName.insert(m_lastCookie, qMakePair<QString, QString>(appName, reason));

    // Retrieve the service, if we've been called from DBus
    if (calledFromDBus() && !m_busWatcher.isNull()) {
        if (!message().service().isEmpty()) {
            m_cookieToBusService.insert(m_lastCookie, message().service());
            m_busWatcher.data()->addWatchedService(message().service());
        }
    }

    kDebug() << "Added inhibition with cookie " << m_lastCookie;

    addInhibitionTypeHelper(m_lastCookie, static_cast< PolicyAgent::RequiredPolicies >(types));

    return m_lastCookie;
}

void PolicyAgent::addInhibitionTypeHelper(uint cookie, PolicyAgent::RequiredPolicies types)
{
    // Look through all of the inhibition types
    bool notify = false;
    if (types & ChangeProfile) {
        // Check if we have to notify
        if (m_typesToCookie[ChangeProfile].isEmpty()) {
            kDebug() << "Added change profile";
            notify = true;
        }
        m_typesToCookie[ChangeProfile].append(cookie);
    }
    if (types & ChangeScreenSettings) {
        // Check if we have to notify
        kDebug() << "Added change screen settings";
        if (m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            notify = true;
        }
        m_typesToCookie[ChangeScreenSettings].append(cookie);
    }
    if (types & InterruptSession) {
        // Check if we have to notify
        kDebug() << "Added interrupt session";
        if (m_typesToCookie[InterruptSession].isEmpty()) {
            notify = true;
        }
        m_typesToCookie[InterruptSession].append(cookie);
    }

    if (notify) {
        // Emit the signal - inhibition has changed
        emit unavailablePoliciesChanged(unavailablePolicies());
    }
}

void PolicyAgent::ReleaseInhibition(uint cookie)
{
    m_cookieToAppName.remove(cookie);
    if (m_cookieToBusService.contains(cookie)) {
        if (!m_busWatcher.isNull()) {
            m_busWatcher.data()->removeWatchedService(m_cookieToBusService[cookie]);
        }
        m_cookieToBusService.remove(cookie);
    }

    // Look through all of the inhibition types
    bool notify = false;
    if (m_typesToCookie[ChangeProfile].contains(cookie)) {
        m_typesToCookie[ChangeProfile].removeOne(cookie);
        // Check if we have to notify
        if (m_typesToCookie[ChangeProfile].isEmpty()) {
            notify = true;
        }
    }
    if (m_typesToCookie[ChangeScreenSettings].contains(cookie)) {
        m_typesToCookie[ChangeScreenSettings].removeOne(cookie);
        // Check if we have to notify
        if (m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            notify = true;
        }
    }
    if (m_typesToCookie[InterruptSession].contains(cookie)) {
        m_typesToCookie[InterruptSession].removeOne(cookie);
        // Check if we have to notify
        if (m_typesToCookie[InterruptSession].isEmpty()) {
            notify = true;
        }
    }

    if (notify) {
        // Emit the signal - inhibition has changed
        emit unavailablePoliciesChanged(unavailablePolicies());
    }
}

void PolicyAgent::releaseAllInhibitions()
{
    QList< uint > allCookies = m_cookieToAppName.keys();
    foreach (uint cookie, allCookies) {
        ReleaseInhibition(cookie);
    }
}

}

#include "powerdevilpolicyagent.moc"
