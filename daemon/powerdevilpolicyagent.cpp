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
{
    Q_ASSERT(!s_globalPolicyAgent->q);
    s_globalPolicyAgent->q = this;
}

PolicyAgent::~PolicyAgent()
{

}

void PolicyAgent::init()
{
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
        // TODO enforce policies here
    }
    if (policies & ChangeScreenSettings) {
        // TODO enforce policies here
    }
    if (policies & InterruptSession) {
        if (m_sessionIsBeingInterrupted) {
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

}
