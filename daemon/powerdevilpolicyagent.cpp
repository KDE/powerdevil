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

#include <KGlobal>
#include <QtDBus/QDBusConnection>
#include <KDebug>
#include <QtDBus/QDBusInterface>
#include <QDBusPendingReply>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include "powermanagementpolicyagentadaptor.h"

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
    , m_sessionIsBeingInterrupted(false)
    , m_lastCookie(0)
{
    Q_ASSERT(!s_globalPolicyAgent->q);
    s_globalPolicyAgent->q = this;
}

PolicyAgent::~PolicyAgent()
{

}

void PolicyAgent::init()
{
    // Start the DBus service
    new PowerManagementPolicyAgentAdaptor(this);

    QDBusConnection::sessionBus().registerService("org.kde.Solid.PowerManagement.PolicyAgent");
    QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement/PolicyAgent", this);

    // Let's cache the needed information to check if our session is actually active
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.ConsoleKit")) {
        // No way to determine if we are on the current session, simply suppose we are
        kDebug() << "Can't contact ck";
        m_ckAvailable = false;
        return;
    } else {
        m_ckAvailable = true;
    }

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

    if (!m_ckSessionInterface->isValid()) {
        // As above
        kDebug() << "Can't contact iface";
        m_ckAvailable = false;
        return;
    }

    // Now set up our service watcher
    m_busWatcher = new QDBusServiceWatcher(this);
    m_busWatcher->setConnection(QDBusConnection::sessionBus());
    m_busWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_busWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onServiceUnregistered(QString)));
}

void PolicyAgent::onServiceUnregistered(const QString& serviceName)
{
    if (m_cookieToBusService.values().contains(serviceName)) {
        // Ouch - the application quit or crashed without releasing its inhibitions. Let's fix that.
        releaseInhibition(m_cookieToBusService.key(serviceName));
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
    } else {
        QDBusPendingReply< bool > rp = m_ckSessionInterface->asyncCall("IsActive");
        rp.waitForFinished();

        if (!rp.value()) {
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

uint PolicyAgent::addInhibition(uint types,
                                const QString& appName,
                                const QString& reason)
{
    ++m_lastCookie;

    m_cookieToAppName.insert(m_lastCookie, qMakePair<QString, QString>(appName, reason));

    addInhibitionTypeHelper(m_lastCookie, static_cast< PolicyAgent::RequiredPolicies >(types));

    return m_lastCookie;
}

uint PolicyAgent::addInhibition(uint types,
                                const QString& appName,
                                const QString& reason,
                                const QString& dbusService)
{
    ++m_lastCookie;

    m_cookieToAppName.insert(m_lastCookie, qMakePair<QString, QString>(appName, reason));
    m_cookieToBusService.insert(m_lastCookie, dbusService);

    // Watch over the bus service: so that we can eventually release the inhibition if the app crashes.
    m_busWatcher->addWatchedService(dbusService);

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
            notify = true;
        }
        m_typesToCookie[ChangeProfile].append(cookie);
    }
    if (types & ChangeScreenSettings) {
        // Check if we have to notify
        if (m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            notify = true;
        }
        m_typesToCookie[ChangeScreenSettings].append(cookie);
    }
    if (types & ChangeProfile) {
        // Check if we have to notify
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

void PolicyAgent::releaseInhibition(uint cookie)
{
    m_cookieToAppName.remove(cookie);
    if (m_cookieToBusService.contains(cookie)) {
        m_busWatcher->removeWatchedService(m_cookieToBusService[cookie]);
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
        releaseInhibition(cookie);
    }
}

}

#include "powerdevilpolicyagent.moc"
