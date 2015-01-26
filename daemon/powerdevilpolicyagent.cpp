/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2012 Lukáš Tinkl <ltinkl@redhat.com>                    *
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

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusArgument>
#include <QtCore/QMetaType>
#include <QtDBus/QDBusMetaType>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusServiceWatcher>

#include "powerdevilpolicyagent.h"
#include "powerdevil_debug.h"

struct NamedDBusObjectPath
{
    QString name;
    QDBusObjectPath path;
};

// Marshall the NamedDBusObjectPath data into a D-Bus argument
QDBusArgument &operator<<(QDBusArgument &argument, const NamedDBusObjectPath &namedPath)
{
    argument.beginStructure();
    argument << namedPath.name << namedPath.path;
    argument.endStructure();
    return argument;
}

// Retrieve the NamedDBusObjectPath data from the D-Bus argument
const QDBusArgument &operator>>(const QDBusArgument &argument, NamedDBusObjectPath &namedPath)
{
    argument.beginStructure();
    argument >> namedPath.name >> namedPath.path;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(NamedDBusObjectPath)
Q_DECLARE_METATYPE(InhibitionInfo)
Q_DECLARE_METATYPE(QList<InhibitionInfo>)

namespace PowerDevil
{

class PolicyAgentHelper
{
public:
    PolicyAgentHelper() : q(0) { }
    ~PolicyAgentHelper() {
        delete q;
    }
    PolicyAgent *q;
};

Q_GLOBAL_STATIC(PolicyAgentHelper, s_globalPolicyAgent)

PolicyAgent *PolicyAgent::instance()
{
    if (!s_globalPolicyAgent->q) {
        new PolicyAgent;
    }

    return s_globalPolicyAgent->q;
}

PolicyAgent::PolicyAgent(QObject* parent)
    : QObject(parent)
    , m_sdAvailable(false)
    , m_systemdInhibitFd(-1)
    , m_ckAvailable(false)
    , m_sessionIsBeingInterrupted(false)
    , m_lastCookie(0)
    , m_busWatcher(new QDBusServiceWatcher(this))
    , m_sdWatcher(new QDBusServiceWatcher(this))
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
    qDBusRegisterMetaType<InhibitionInfo>();
    qDBusRegisterMetaType<QList<InhibitionInfo>>();

    // Watch over the systemd service
    m_sdWatcher.data()->setConnection(QDBusConnection::systemBus());
    m_sdWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration |
                                     QDBusServiceWatcher::WatchForRegistration);
    m_sdWatcher.data()->addWatchedService(SYSTEMD_LOGIN1_SERVICE);

    connect(m_sdWatcher.data(), SIGNAL(serviceRegistered(QString)),
            this, SLOT(onSessionHandlerRegistered(QString)));
    connect(m_sdWatcher.data(), SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onSessionHandlerUnregistered(QString)));
    // If it's up and running already, let's cache it
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(SYSTEMD_LOGIN1_SERVICE)) {
        onSessionHandlerRegistered(SYSTEMD_LOGIN1_SERVICE);
    }

    // Watch over the ConsoleKit service
    m_ckWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_ckWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration |
                                     QDBusServiceWatcher::WatchForRegistration);
    m_ckWatcher.data()->addWatchedService(CONSOLEKIT_SERVICE);

    connect(m_ckWatcher.data(), SIGNAL(serviceRegistered(QString)),
            this, SLOT(onSessionHandlerRegistered(QString)));
    connect(m_ckWatcher.data(), SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onSessionHandlerUnregistered(QString)));
    // If it's up and running already, let's cache it
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT_SERVICE)) {
        onSessionHandlerRegistered(CONSOLEKIT_SERVICE);
    }

    // Now set up our service watcher
    m_busWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_busWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_busWatcher.data(), SIGNAL(serviceUnregistered(QString)),
            this, SLOT(onServiceUnregistered(QString)));
}

QString PolicyAgent::getNamedPathProperty(const QString &path, const QString &iface, const QString &prop) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(SYSTEMD_LOGIN1_SERVICE, path,
                                                          QLatin1String("org.freedesktop.DBus.Properties"), QLatin1String("Get"));
    message << iface << prop;
    QDBusMessage reply = QDBusConnection::systemBus().call(message);

    QVariantList args = reply.arguments();
    if (!args.isEmpty()) {
        NamedDBusObjectPath namedPath;
        args.at(0).value<QDBusVariant>().variant().value<QDBusArgument>() >> namedPath;
        return namedPath.path.path();
    }

    return QString();
}

void PolicyAgent::onSessionHandlerRegistered(const QString & serviceName)
{
    if (serviceName == SYSTEMD_LOGIN1_SERVICE) {
        m_sdAvailable = true;

        qRegisterMetaType<NamedDBusObjectPath>();
        qDBusRegisterMetaType<NamedDBusObjectPath>();

        // get the current session
        QDBusInterface managerIface(SYSTEMD_LOGIN1_SERVICE, SYSTEMD_LOGIN1_PATH, SYSTEMD_LOGIN1_MANAGER_IFACE, QDBusConnection::systemBus());
        QDBusPendingReply<QDBusObjectPath> session = managerIface.asyncCall(QLatin1String("GetSessionByPID"), (quint32) QCoreApplication::applicationPid());
        session.waitForFinished();

        if (!session.isValid()) {
            qCDebug(POWERDEVIL) << "The session is not registered with systemd";
            m_sdAvailable = false;
            return;
        }

        QString sessionPath = session.value().path();
        qCDebug(POWERDEVIL) << "Session path:" << sessionPath;

        m_sdSessionInterface = new QDBusInterface(SYSTEMD_LOGIN1_SERVICE, sessionPath,
                                                  SYSTEMD_LOGIN1_SESSION_IFACE, QDBusConnection::systemBus(), this);
        if (!m_sdSessionInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact session iface";
            m_sdAvailable = false;
            delete m_sdSessionInterface.data();
            return;
        }

        // now let's obtain the seat
        QString seatPath = getNamedPathProperty(sessionPath, SYSTEMD_LOGIN1_SESSION_IFACE, "Seat");

        if (seatPath.isEmpty() || seatPath == "/") {
            qCDebug(POWERDEVIL) << "Unable to associate systemd session with a seat" << seatPath;
            m_sdAvailable = false;
            return;
        }

        // get the current seat
        m_sdSeatInterface = new QDBusInterface(SYSTEMD_LOGIN1_SERVICE, seatPath,
                                               SYSTEMD_LOGIN1_SEAT_IFACE, QDBusConnection::systemBus(), this);

        if (!m_sdSeatInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact seat iface";
            m_sdAvailable = false;
            delete m_sdSeatInterface.data();
            return;
        }

        // finally get the active session path and watch for its changes
        m_activeSessionPath = getNamedPathProperty(seatPath, SYSTEMD_LOGIN1_SEAT_IFACE, "ActiveSession");

        qCDebug(POWERDEVIL) << "ACTIVE SESSION PATH:" << m_activeSessionPath;
        QDBusConnection::systemBus().connect(SYSTEMD_LOGIN1_SERVICE, seatPath, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                             SLOT(onActiveSessionChanged(QString,QVariantMap,QStringList)));

        onActiveSessionChanged(m_activeSessionPath);

        setupSystemdInhibition();

        qCDebug(POWERDEVIL) << "systemd support initialized";
    } else if (serviceName == CONSOLEKIT_SERVICE) {
        m_ckAvailable = true;

        // Otherwise, let's ask ConsoleKit
        QDBusInterface ckiface(CONSOLEKIT_SERVICE, "/org/freedesktop/ConsoleKit/Manager",
                               "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());

        QDBusPendingReply<QDBusObjectPath> sessionPath = ckiface.asyncCall("GetCurrentSession");

        sessionPath.waitForFinished();

        if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
            qCDebug(POWERDEVIL) << "The session is not registered with ck";
            m_ckAvailable = false;
            return;
        }

        m_ckSessionInterface = new QDBusInterface(CONSOLEKIT_SERVICE, sessionPath.value().path(),
                                                  "org.freedesktop.ConsoleKit.Session", QDBusConnection::systemBus());

        if (!m_ckSessionInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact iface";
            m_ckAvailable = false;
            return;
        }

        // Now let's obtain the seat
        QDBusPendingReply< QDBusObjectPath > seatPath = m_ckSessionInterface.data()->asyncCall("GetSeatId");
        seatPath.waitForFinished();

        if (!seatPath.isValid() || seatPath.value().path().isEmpty()) {
            qCDebug(POWERDEVIL) << "Unable to associate ck session with a seat";
            m_ckAvailable = false;
            return;
        }

        if (!QDBusConnection::systemBus().connect(CONSOLEKIT_SERVICE, seatPath.value().path(),
                                                  "org.freedesktop.ConsoleKit.Seat", "ActiveSessionChanged",
                                                  this, SLOT(onActiveSessionChanged(QString)))) {
            qCDebug(POWERDEVIL) << "Unable to connect to ActiveSessionChanged";
            m_ckAvailable = false;
            return;
        }

        // Force triggering of active session changed
        QDBusMessage call = QDBusMessage::createMethodCall(CONSOLEKIT_SERVICE, seatPath.value().path(),
                                                           "org.freedesktop.ConsoleKit.Seat", "GetActiveSession");
        QDBusPendingReply< QDBusObjectPath > activeSession = QDBusConnection::systemBus().asyncCall(call);
        activeSession.waitForFinished();

        onActiveSessionChanged(activeSession.value().path());

        qCDebug(POWERDEVIL) << "ConsoleKit support initialized";
    }
    else
        qCWarning(POWERDEVIL) << "Unhandled service registered:" << serviceName;
}

void PolicyAgent::onSessionHandlerUnregistered(const QString & serviceName)
{
    if (serviceName == SYSTEMD_LOGIN1_SERVICE) {
        m_sdAvailable = false;
        delete m_sdSessionInterface.data();
    }
    else if (serviceName == CONSOLEKIT_SERVICE) {
        m_ckAvailable = false;
        delete m_ckSessionInterface.data();
    }
}

void PolicyAgent::onActiveSessionChanged(const QString & ifaceName, const QVariantMap & changedProps, const QStringList & invalidatedProps)
{
    const QString key = QLatin1String("ActiveSession");

    if (ifaceName == SYSTEMD_LOGIN1_SEAT_IFACE && (changedProps.keys().contains(key) || invalidatedProps.contains(key))) {
        m_activeSessionPath = getNamedPathProperty(m_sdSeatInterface.data()->path(), SYSTEMD_LOGIN1_SEAT_IFACE, key);
        qCDebug(POWERDEVIL) << "ACTIVE SESSION PATH CHANGED:" << m_activeSessionPath;
        onActiveSessionChanged(m_activeSessionPath);
    }
}

void PolicyAgent::onActiveSessionChanged(const QString& activeSession)
{
    if (activeSession.isEmpty() || activeSession == "/") {
        qCDebug(POWERDEVIL) << "Switched to inactive session - leaving unchanged";
        return;
    } else if ((!m_sdSessionInterface.isNull() && activeSession == m_sdSessionInterface.data()->path()) ||
               (!m_ckSessionInterface.isNull() && activeSession == m_ckSessionInterface.data()->path())) {
        qCDebug(POWERDEVIL) << "Current session is now active";
        m_wasLastActiveSession = true;
    } else {
        qCDebug(POWERDEVIL) << "Current session is now inactive";
        m_wasLastActiveSession = false;
    }
}

void PolicyAgent::onServiceUnregistered(const QString& serviceName)
{
    if (m_cookieToBusService.values().contains(serviceName)) {
        // Ouch - the application quit or crashed without releasing its inhibitions. Let's fix that.
        foreach (uint key, m_cookieToBusService.keys(serviceName)) {
            ReleaseInhibition(key);
        }
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
    if (!m_sdAvailable) {
        // No way to determine if we are on the current session, simply suppose we are
        qCDebug(POWERDEVIL) << "Can't contact systemd";
    } else if (!m_sdSessionInterface.isNull()) {
        bool isActive = m_sdSessionInterface.data()->property("Active").toBool();

        if (!isActive && !m_wasLastActiveSession) {
            return policies;
        }
    }

    if (!m_ckAvailable) {
        // No way to determine if we are on the current session, simply suppose we are
        qCDebug(POWERDEVIL) << "Can't contact ck";
    } else if (!m_ckSessionInterface.isNull()) {
        QDBusPendingReply< bool > rp = m_ckSessionInterface.data()->asyncCall("IsActive");
        rp.waitForFinished();

        if (!(rp.isValid() && rp.value()) && !m_wasLastActiveSession) {
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

    qCDebug(POWERDEVIL) << "Added inhibition from an explicit DBus service, " << service << ", with cookie " <<
            m_lastCookie << " from " << appName << " with " << reason;

    addInhibitionTypeHelper(m_lastCookie, static_cast< PolicyAgent::RequiredPolicies >(types));

    emit InhibitionsChanged({ {qMakePair(appName, reason)} }, QStringList());

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
            qCDebug(POWERDEVIL) << "DBus service " << message().service() << " is requesting inhibition";
            m_cookieToBusService.insert(m_lastCookie, message().service());
            m_busWatcher.data()->addWatchedService(message().service());
        }
    }

    qCDebug(POWERDEVIL) << "Added inhibition with cookie " << m_lastCookie << " from " <<
            appName << " with " << reason;

    addInhibitionTypeHelper(m_lastCookie, static_cast< PolicyAgent::RequiredPolicies >(types));

    emit InhibitionsChanged({ {qMakePair(appName, reason)} }, QStringList());

    return m_lastCookie;
}



void PolicyAgent::addInhibitionTypeHelper(uint cookie, PolicyAgent::RequiredPolicies types)
{
    // Look through all of the inhibition types
    bool notify = false;
    if (types & ChangeProfile) {
        // Check if we have to notify
        if (m_typesToCookie[ChangeProfile].isEmpty()) {
            qCDebug(POWERDEVIL) << "Added change profile";
            notify = true;
        }
        m_typesToCookie[ChangeProfile].append(cookie);
    }
    if (types & ChangeScreenSettings) {
        // Check if we have to notify
        qCDebug(POWERDEVIL) << "Added change screen settings";
        if (m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            notify = true;
        }
        m_typesToCookie[ChangeScreenSettings].append(cookie);
        types |= InterruptSession;  // implied by ChangeScreenSettings
    }
    if (types & InterruptSession) {
        // Check if we have to notify
        qCDebug(POWERDEVIL) << "Added interrupt session";
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
    qCDebug(POWERDEVIL) << "Released inhibition with cookie " << cookie;
    emit InhibitionsChanged(QList<InhibitionInfo>(), { {m_cookieToAppName.value(cookie).first} });
    m_cookieToAppName.remove(cookie);


    QString service = m_cookieToBusService.take(cookie);
    if (!m_busWatcher.isNull() && !service.isEmpty() && !m_cookieToBusService.key(service)) {
        // no cookies from service left
        m_busWatcher.data()->removeWatchedService(service);
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

QList<InhibitionInfo> PolicyAgent::ListInhibitions() const
{
    return m_cookieToAppName.values();
}

void PolicyAgent::releaseAllInhibitions()
{
    QList< uint > allCookies = m_cookieToAppName.keys();
    foreach (uint cookie, allCookies) {
        ReleaseInhibition(cookie);
    }
}

void PolicyAgent::setupSystemdInhibition()
{
    if (m_systemdInhibitFd.fileDescriptor() != -1)
        return;

    // inhibit systemd handling of power/sleep/lid buttons
    // http://www.freedesktop.org/wiki/Software/systemd/inhibit
    QDBusInterface managerIface(SYSTEMD_LOGIN1_SERVICE, SYSTEMD_LOGIN1_PATH, SYSTEMD_LOGIN1_MANAGER_IFACE, QDBusConnection::systemBus());
    qCDebug(POWERDEVIL) << "fd passing available:" << bool(managerIface.connection().connectionCapabilities() & QDBusConnection::UnixFileDescriptorPassing);

    QVariantList args;
    args << "handle-power-key:handle-suspend-key:handle-hibernate-key:handle-lid-switch"; // what
    args << "PowerDevil"; // who
    args << "KDE handles power events"; // why
    args << "block"; // mode
    QDBusPendingReply<QDBusUnixFileDescriptor> desc = managerIface.asyncCallWithArgumentList("Inhibit", args);
    desc.waitForFinished();
    if (desc.isValid()) {
        m_systemdInhibitFd = desc.value();
        qCDebug(POWERDEVIL) << "systemd powersave events handling inhibited, descriptor:" << m_systemdInhibitFd.fileDescriptor();
    }
    else
        qCWarning(POWERDEVIL) << "failed to inhibit systemd powersave handling";
}

}
