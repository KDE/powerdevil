/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2012 Lukáš Tinkl <ltinkl@redhat.com>
 *   SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 *   SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QPair>
#include <QTimer>
#include <QVariant>

#include <KConfigGroup>
#include <KIdleTime>

#include <algorithm>
#include <unistd.h>

#include "powerdevil_debug.h"
#include "powerdevilpolicyagent.h"

#include "screenlocker_interface.h"

using namespace Qt::StringLiterals;

struct NamedDBusObjectPath {
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

QDBusArgument &operator<<(QDBusArgument &argument, const LogindInhibition &inhibition)
{
    argument.beginStructure();
    argument << inhibition.what << inhibition.who << inhibition.why << inhibition.mode << inhibition.uid << inhibition.pid;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, LogindInhibition &inhibition)
{
    argument.beginStructure();
    argument >> inhibition.what >> inhibition.who >> inhibition.why >> inhibition.mode >> inhibition.uid >> inhibition.pid;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PolicyAgentInhibition &inhibition)
{
    argument.beginStructure();
    argument << inhibition.what << inhibition.who << inhibition.why << inhibition.mode << inhibition.flags;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PolicyAgentInhibition &inhibition)
{
    argument.beginStructure();
    argument >> inhibition.what >> inhibition.who >> inhibition.why >> inhibition.mode >> inhibition.flags;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(NamedDBusObjectPath)
Q_DECLARE_METATYPE(LogindInhibition)
Q_DECLARE_METATYPE(QList<LogindInhibition>)
Q_DECLARE_METATYPE(PolicyAgentInhibition)
Q_DECLARE_METATYPE(QList<PolicyAgentInhibition>)

namespace PowerDevil
{
static const QString SCREEN_LOCKER_SERVICE_NAME = QStringLiteral("org.freedesktop.ScreenSaver");

class PolicyAgentHelper
{
public:
    PolicyAgentHelper()
        : q(nullptr)
    {
    }
    ~PolicyAgentHelper()
    {
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

PolicyAgent::PolicyAgent(QObject *parent)
    : QObject(parent)
    , m_sdAvailable(false)
    , m_systemdInhibitFd(-1)
    , m_ckAvailable(false)
    , m_sessionIsBeingInterrupted(false)
    , m_lastCookie(0)
    , m_busWatcher(new QDBusServiceWatcher(this))
    , m_sdWatcher(new QDBusServiceWatcher(this))
    , m_ckWatcher(new QDBusServiceWatcher(this))
    , m_wasLastActiveSession(false)
{
    Q_ASSERT(!s_globalPolicyAgent->q);
    s_globalPolicyAgent->q = this;
}

PolicyAgent::~PolicyAgent()
{
}

void PolicyAgent::init(GlobalSettings *globalSettings)
{
    qDBusRegisterMetaType<PolicyAgentInhibition>();
    qDBusRegisterMetaType<QList<PolicyAgentInhibition>>();

    // DEPRECATED: for ListInhibitions, InhibitionsChanged
    qDBusRegisterMetaType<QList<QStringList>>();

    // Watch over the systemd service
    m_sdWatcher.data()->setConnection(QDBusConnection::systemBus());
    m_sdWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration | QDBusServiceWatcher::WatchForRegistration);
    m_sdWatcher.data()->addWatchedService(SYSTEMD_LOGIN1_SERVICE);

    connect(m_sdWatcher.data(), &QDBusServiceWatcher::serviceRegistered, this, &PolicyAgent::onSessionHandlerRegistered);
    connect(m_sdWatcher.data(), &QDBusServiceWatcher::serviceUnregistered, this, &PolicyAgent::onSessionHandlerUnregistered);
    // If it's up and running already, let's cache it
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(SYSTEMD_LOGIN1_SERVICE)) {
        onSessionHandlerRegistered(SYSTEMD_LOGIN1_SERVICE);
    }

    // Watch over the ConsoleKit service
    m_ckWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_ckWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration | QDBusServiceWatcher::WatchForRegistration);
    m_ckWatcher.data()->addWatchedService(CONSOLEKIT_SERVICE);

    connect(m_ckWatcher.data(), &QDBusServiceWatcher::serviceRegistered, this, &PolicyAgent::onSessionHandlerRegistered);
    connect(m_ckWatcher.data(), &QDBusServiceWatcher::serviceUnregistered, this, &PolicyAgent::onSessionHandlerUnregistered);
    // If it's up and running already, let's cache it
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT_SERVICE)) {
        onSessionHandlerRegistered(CONSOLEKIT_SERVICE);
    }

    // Now set up our service watcher
    m_busWatcher.data()->setConnection(QDBusConnection::sessionBus());
    m_busWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_busWatcher.data(), &QDBusServiceWatcher::serviceUnregistered, this, &PolicyAgent::onServiceUnregistered);

    // Setup the screen locker watcher and check whether the screen is currently locked
    m_screenLockerInterface =
        new OrgFreedesktopScreenSaverInterface(SCREEN_LOCKER_SERVICE_NAME, QStringLiteral("/ScreenSaver"), QDBusConnection::sessionBus(), this);
    connect(m_screenLockerInterface, &OrgFreedesktopScreenSaverInterface::ActiveChanged, this, &PolicyAgent::onScreenLockerActiveChanged);

    // Emit PropertiesChanged D-Bus signal for interface properties, because QDBus doesn't do that
    connect(this, &PolicyAgent::dbusPropertiesChanged, this, [this](const QStringList &props) {
        QDBusMessage signal =
            QDBusMessage::createSignal("/org/kde/Solid/PowerManagement/PolicyAgent"_L1, "org.freedesktop.DBus.Properties"_L1, "PropertiesChanged"_L1);
        signal << u"org.kde.Solid.PowerManagement.PolicyAgent"_s << QVariantMap() << props;
        QDBusConnection::sessionBus().send(signal);
    });

    m_config = globalSettings;
    // get comma separated list of colon separated "app:reason" inhibition pairs
    // that have been configured to be suppressed
    QStringList blockedInhibitions = m_config->blockedInhibitions();
    std::ranges::transform(std::as_const(blockedInhibitions),
                           std::inserter(m_configuredToBlockInhibitions, m_configuredToBlockInhibitions.begin()),
                           [](const QString &inhibition) {
                               QString who = inhibition.section(QLatin1Char(':'), 0, 0);
                               QString why = inhibition.section(QLatin1Char(':'), 1);
                               return qMakePair(std::move(who), std::move(why));
                           });
}

QString PolicyAgent::getNamedPathProperty(const QString &path, const QString &iface, const QString &prop) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(SYSTEMD_LOGIN1_SERVICE, path, QLatin1String("org.freedesktop.DBus.Properties"), QLatin1String("Get"));
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

void PolicyAgent::onSessionHandlerRegistered(const QString &serviceName)
{
    if (serviceName == SYSTEMD_LOGIN1_SERVICE) {
        m_sdAvailable = true;

        qRegisterMetaType<NamedDBusObjectPath>();
        qDBusRegisterMetaType<NamedDBusObjectPath>();
        qDBusRegisterMetaType<LogindInhibition>();
        qDBusRegisterMetaType<QList<LogindInhibition>>();

        // get the current session
        m_managerIface.reset(new QDBusInterface(SYSTEMD_LOGIN1_SERVICE, SYSTEMD_LOGIN1_PATH, SYSTEMD_LOGIN1_MANAGER_IFACE, QDBusConnection::systemBus()));

        if (!m_managerIface.data()->isValid()) {
            qCDebug(POWERDEVIL) << "Can't connect to systemd";
            m_sdAvailable = false;
            return;
        }

        QDBusPendingReply<QDBusObjectPath> session = m_managerIface.data()->asyncCall(QLatin1String("GetSession"), QLatin1String("auto"));
        session.waitForFinished();

        if (!session.isValid()) {
            qCDebug(POWERDEVIL) << "The session is not registered with systemd";
            m_sdAvailable = false;
            return;
        }

        QString sessionPath = session.value().path();
        qCDebug(POWERDEVIL) << "Session path:" << sessionPath;

        m_sdSessionInterface = new QDBusInterface(SYSTEMD_LOGIN1_SERVICE, sessionPath, SYSTEMD_LOGIN1_SESSION_IFACE, QDBusConnection::systemBus(), this);
        if (!m_sdSessionInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact session iface";
            m_sdAvailable = false;
            delete m_sdSessionInterface.data();
            return;
        }

        // now let's obtain the seat
        QString seatPath = getNamedPathProperty(sessionPath, SYSTEMD_LOGIN1_SESSION_IFACE, u"Seat"_s);

        if (seatPath.isEmpty() || seatPath == u'/') {
            qCDebug(POWERDEVIL) << "Unable to associate systemd session with a seat" << seatPath;
            m_sdAvailable = false;
            return;
        }

        // get the current seat
        m_sdSeatInterface = new QDBusInterface(SYSTEMD_LOGIN1_SERVICE, seatPath, SYSTEMD_LOGIN1_SEAT_IFACE, QDBusConnection::systemBus(), this);

        if (!m_sdSeatInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact seat iface";
            m_sdAvailable = false;
            delete m_sdSeatInterface.data();
            return;
        }

        // finally get the active session path and watch for its changes
        m_activeSessionPath = getNamedPathProperty(seatPath, SYSTEMD_LOGIN1_SEAT_IFACE, u"ActiveSession"_s);

        qCDebug(POWERDEVIL) << "ACTIVE SESSION PATH:" << m_activeSessionPath;
        QDBusConnection::systemBus().connect(SYSTEMD_LOGIN1_SERVICE,
                                             seatPath,
                                             u"org.freedesktop.DBus.Properties"_s,
                                             u"PropertiesChanged"_s,
                                             this,
                                             SLOT(onActiveSessionChanged(QString, QVariantMap, QStringList)));

        onActiveSessionChanged(m_activeSessionPath);

        // block logind from handling time-based inhibitions
        setupSystemdInhibition();
        // and then track logind's inhibitions, too
        QDBusConnection::systemBus().connect(SYSTEMD_LOGIN1_SERVICE,
                                             SYSTEMD_LOGIN1_PATH,
                                             u"org.freedesktop.DBus.Properties"_s,
                                             u"PropertiesChanged"_s,
                                             this,
                                             SLOT(onManagerPropertyChanged(QString, QVariantMap, QStringList)));
        checkLogindInhibitions();

        qCDebug(POWERDEVIL) << "systemd support initialized";
    } else if (serviceName == CONSOLEKIT_SERVICE) {
        m_ckAvailable = true;

        // Otherwise, let's ask ConsoleKit
        m_managerIface.reset(new QDBusInterface(CONSOLEKIT_SERVICE, CONSOLEKIT_MANAGER_PATH, CONSOLEKIT_MANAGER_IFACE, QDBusConnection::systemBus()));

        if (!m_managerIface.data()->isValid()) {
            qCDebug(POWERDEVIL) << "Can't connect to ConsoleKit";
            m_ckAvailable = false;
            return;
        }

        QDBusPendingReply<QDBusObjectPath> sessionPath = m_managerIface.data()->asyncCall(u"GetCurrentSession"_s);

        sessionPath.waitForFinished();

        if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
            qCDebug(POWERDEVIL) << "The session is not registered with ck";
            m_ckAvailable = false;
            return;
        }

        m_ckSessionInterface =
            new QDBusInterface(CONSOLEKIT_SERVICE, sessionPath.value().path(), u"org.freedesktop.ConsoleKit.Session"_s, QDBusConnection::systemBus());

        if (!m_ckSessionInterface.data()->isValid()) {
            // As above
            qCDebug(POWERDEVIL) << "Can't contact iface";
            m_ckAvailable = false;
            return;
        }

        // Now let's obtain the seat
        QDBusPendingReply<QDBusObjectPath> seatPath = m_ckSessionInterface.data()->asyncCall(QStringLiteral("GetSeatId"));
        seatPath.waitForFinished();

        if (!seatPath.isValid() || seatPath.value().path().isEmpty()) {
            qCDebug(POWERDEVIL) << "Unable to associate ck session with a seat";
            m_ckAvailable = false;
            return;
        }

        if (!QDBusConnection::systemBus().connect(CONSOLEKIT_SERVICE,
                                                  seatPath.value().path(),
                                                  u"org.freedesktop.ConsoleKit.Seat"_s,
                                                  u"ActiveSessionChanged"_s,
                                                  this,
                                                  SLOT(onActiveSessionChanged(QString)))) {
            qCDebug(POWERDEVIL) << "Unable to connect to ActiveSessionChanged";
            m_ckAvailable = false;
            return;
        }

        // Force triggering of active session changed
        QDBusMessage call =
            QDBusMessage::createMethodCall(CONSOLEKIT_SERVICE, seatPath.value().path(), u"org.freedesktop.ConsoleKit.Seat"_s, u"GetActiveSession"_s);
        QDBusPendingReply<QDBusObjectPath> activeSession = QDBusConnection::systemBus().asyncCall(call);
        activeSession.waitForFinished();

        onActiveSessionChanged(activeSession.value().path());

        setupSystemdInhibition();

        qCDebug(POWERDEVIL) << "ConsoleKit support initialized";
    } else
        qCWarning(POWERDEVIL) << "Unhandled service registered:" << serviceName;
}

void PolicyAgent::onSessionHandlerUnregistered(const QString &serviceName)
{
    if (serviceName == SYSTEMD_LOGIN1_SERVICE) {
        m_sdAvailable = false;
        delete m_sdSessionInterface.data();
    } else if (serviceName == QLatin1String(CONSOLEKIT_SERVICE)) {
        m_ckAvailable = false;
        delete m_ckSessionInterface.data();
    }
}

void PolicyAgent::onActiveSessionChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    const QString key = QLatin1String("ActiveSession");

    if (ifaceName == SYSTEMD_LOGIN1_SEAT_IFACE && (changedProps.contains(key) || invalidatedProps.contains(key))) {
        m_activeSessionPath = getNamedPathProperty(m_sdSeatInterface.data()->path(), SYSTEMD_LOGIN1_SEAT_IFACE, key);
        qCDebug(POWERDEVIL) << "ACTIVE SESSION PATH CHANGED:" << m_activeSessionPath;
        onActiveSessionChanged(m_activeSessionPath);
    }
}

void PolicyAgent::onActiveSessionChanged(const QString &activeSession)
{
    if (activeSession.isEmpty() || activeSession == QLatin1String("/")) {
        qCDebug(POWERDEVIL) << "Switched to inactive session - leaving unchanged";
        return;
    } else if ((!m_sdSessionInterface.isNull() && activeSession == m_sdSessionInterface.data()->path())
               || (!m_ckSessionInterface.isNull() && activeSession == m_ckSessionInterface.data()->path())) {
        qCDebug(POWERDEVIL) << "Current session is now active";
        if (!m_wasLastActiveSession) {
            m_wasLastActiveSession = true;
            Q_EMIT sessionActiveChanged(true);
        }
    } else {
        qCDebug(POWERDEVIL) << "Current session is now inactive";
        if (m_wasLastActiveSession) {
            m_wasLastActiveSession = false;
            Q_EMIT sessionActiveChanged(false);
        }
    }
}

PolicyAgent::RequiredPolicies PolicyAgent::policiesInLogindWhat(const QString &what) const
{
    const auto types = what.split(QLatin1Char(':'));

    RequiredPolicies policies{};

    if (types.contains("sleep"_L1)) {
        policies |= InterruptSession;
    }
    if (types.contains("idle"_L1)) {
        policies |= ChangeScreenSettings;
    }
    return policies;
}

QString PolicyAgent::logindWhatForPolicies(PolicyAgent::RequiredPolicies policies) const
{
    QStringList what;
    if (policies & InterruptSession) {
        what.append(u"sleep"_s);
    }
    if (policies & ChangeScreenSettings) {
        what.append(u"idle"_s);
    }
    return what.join(":"_L1);
}

void PolicyAgent::checkLogindInhibitions()
{
    qCDebug(POWERDEVIL) << "Checking logind inhibitions";

    QDBusPendingReply<QList<LogindInhibition>> reply = m_managerIface->asyncCall(QStringLiteral("ListInhibitors"));

    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QList<LogindInhibition>> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            qCWarning(POWERDEVIL) << "Failed to ask logind for inhibitions" << reply.error().message();
            return;
        }

        const auto activeInhibitions = reply.value();

        // Add all inhibitions that we don't know already
        for (const auto &activeInhibition : activeInhibitions) {
            if (activeInhibition.mode != QLatin1String("block")) {
                continue;
            }

            if (static_cast<pid_t>(activeInhibition.pid) == getpid()) {
                continue;
            }

            RequiredPolicies policies = policiesInLogindWhat(activeInhibition.what);
            if (!policies) {
                continue;
            }

            const bool known = std::find(m_logindInhibitions.constBegin(), m_logindInhibitions.constEnd(), activeInhibition) != m_logindInhibitions.constEnd();
            if (known) {
                continue;
            }

            qCDebug(POWERDEVIL) << "Adding logind inhibition:" << activeInhibition.what << activeInhibition.who << activeInhibition.why << "from"
                                << activeInhibition.pid << "of user" << activeInhibition.uid;
            const uint cookie = AddInhibition(policies, activeInhibition.who, activeInhibition.why);
            m_logindInhibitions.insert(cookie, activeInhibition);
        }

        // Remove all inhibitions that logind doesn't have anymore
        for (auto it = m_logindInhibitions.begin(); it != m_logindInhibitions.end();) {
            if (!activeInhibitions.contains(*it)) {
                qCDebug(POWERDEVIL) << "Releasing logind inhibition:" << it->what << it->who << it->why << "from" << it->pid << "of user" << it->uid;
                ReleaseInhibition(it.key());
                it = m_logindInhibitions.erase(it);
            } else {
                ++it;
            }
        }
    });
}

void PolicyAgent::onManagerPropertyChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    const QString key = QStringLiteral("BlockInhibited");

    if (ifaceName == SYSTEMD_LOGIN1_MANAGER_IFACE && (changedProps.contains(key) || invalidatedProps.contains(key))) {
        checkLogindInhibitions();
    }
}

void PolicyAgent::onServiceUnregistered(const QString &serviceName)
{
    // Ouch - the application quit or crashed without releasing its inhibitions. Let's fix that.

    // ReleaseInhibition removes the cookies from the hash, so we need to operate on a copy
    const auto cookieToBusService = m_cookieToBusService;
    for (auto it = cookieToBusService.constBegin(); it != cookieToBusService.constEnd(); ++it) {
        if (it.value() == serviceName) {
            ReleaseInhibition(it.key());
        }
    }
}

PolicyAgent::RequiredPolicies PolicyAgent::unavailablePolicies()
{
    RequiredPolicies retpolicies = None;

    // when screen locker is active it makes no sense to keep the screen on
    if (!m_screenLockerActive && !m_typesToCookie[ChangeScreenSettings].isEmpty()) {
        retpolicies |= ChangeScreenSettings;
    }
    if (!m_typesToCookie[InterruptSession].isEmpty()) {
        retpolicies |= InterruptSession;
    }

    return retpolicies;
}

bool PolicyAgent::screenLockerActive() const
{
    return m_screenLockerActive;
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
        QDBusPendingReply<bool> rp = m_ckSessionInterface.data()->asyncCall(QStringLiteral("IsActive"));
        rp.waitForFinished();

        if (!(rp.isValid() && rp.value()) && !m_wasLastActiveSession) {
            return policies;
        }
    }

    // Ok, let's go then
    RequiredPolicies retpolicies = None;

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

void PolicyAgent::onScreenLockerActiveChanged(bool active)
{
    const auto oldPolicies = unavailablePolicies();

    if (m_screenLockerActive != active) {
        m_screenLockerActive = active;
        Q_EMIT screenLockerActiveChanged(active);
    }

    const auto newPolicies = unavailablePolicies();

    if (oldPolicies != newPolicies) {
        qCDebug(POWERDEVIL) << "Screen saver active" << active << "- we have different inhibition policy now because of that";
        Q_EMIT unavailablePoliciesChanged(newPolicies);
    }
}

uint PolicyAgent::addInhibitionWithExplicitDBusService(uint types, const QString &appName, const QString &reason, const QString &service)
{
    const InhibitionInternals inhibition{
        .policies = static_cast<PolicyAgent::RequiredPolicies>(types),
        .who = appName,
        .why = reason,
    };

    ++m_lastCookie;

    const int cookie = m_lastCookie; // when the Timer below fires, m_lastCookie might be different already

    if (!m_busWatcher.isNull() && !service.isEmpty()) {
        m_cookieToBusService.insert(cookie, service);
        m_busWatcher.data()->addWatchedService(service);
    }

    m_pendingInhibitions.insert(cookie);

    qCDebug(POWERDEVIL) << "Scheduling inhibition from" << service << appName << "with cookie" << cookie << "and reason" << reason;

    // wait 5s before actually enforcing the inhibition
    // there might be short interruptions (such as receiving a message) where an app might automatically
    // post an inhibition but we don't want the system to constantly wakeup because of this
    QTimer::singleShot(5000, this, [this, cookie, service, inhibition] {
        qCDebug(POWERDEVIL) << "Enforcing inhibition from" << service << inhibition.who << "with cookie" << cookie << "and reason" << inhibition.why;

        if (!m_pendingInhibitions.contains(cookie)) {
            qCDebug(POWERDEVIL) << "By the time we wanted to enforce the inhibition it was already gone; discarding it";
            return;
        }

        m_cookieToInhibition.insert(cookie, inhibition);

        m_inhibitionAllowedChangedConnections.insert(
            cookie,
            connect(this,
                    &PolicyAgent::inhibitionAllowedChanged,
                    this,
                    [this, cookie, service, inhibition](const RequestedInhibitionId &requested, bool allowed) {
                        const RequestedInhibitionId thisId = qMakePair(inhibition.who, inhibition.why);
                        if (thisId != requested) {
                            return;
                        }

                        // when inhibition gets blocked later, revoke it if it is still active
                        if (!allowed && m_activeInhibitions.contains(cookie)) {
                            qCDebug(POWERDEVIL) << "Inhibition with cookie" << cookie << "from" << service << inhibition.who << "with reason" << inhibition.why
                                                << "was configured by the user to be blocked; releasing";

                            // remove m_activeInhibitions[cookie], notify D-Bus properties, retain cookie
                            ReleaseInhibition(cookie, true);
                        }

                        // when inhibition is allowed again later, retroactively add it if it still exists
                        if (allowed && !m_activeInhibitions.contains(cookie) && m_cookieToInhibition.contains(cookie)) {
                            qCDebug(POWERDEVIL) << "Inhibition with cookie" << cookie << "from" << service << inhibition.who << "with reason" << inhibition.why
                                                << "was configured by the user to be allowed again; adding retroactively";

                            addInhibitionTypeHelper(cookie, inhibition.policies);
                            m_activeInhibitions.insert(cookie);
                            Q_EMIT dbusPropertiesChanged({u"RequestedInhibitions"_s, u"ActiveInhibitions"_s});
                            Q_EMIT InhibitionsChanged({{QStringList{inhibition.who, inhibition.why}}}, {});
                        }
                    }));

        // reject inhibitions configured as blocked
        const RequestedInhibitionId id = qMakePair(inhibition.who, inhibition.why);
        m_pendingInhibitions.remove(cookie);
        m_requestedInhibitions.append(cookie);

        if (m_configuredToBlockInhibitions.contains(id)) {
            qCDebug(POWERDEVIL) << "Inhibition with cookie" << cookie << "from" << service << inhibition.who << "with reason" << inhibition.why
                                << "has been configured by the user to be blocked; rejecting";

            Q_EMIT dbusPropertiesChanged({u"RequestedInhibitions"_s});
            return;
        }

        addInhibitionTypeHelper(cookie, inhibition.policies);
        m_activeInhibitions.insert(cookie);
        Q_EMIT dbusPropertiesChanged({u"RequestedInhibitions"_s, u"ActiveInhibitions"_s});
        Q_EMIT InhibitionsChanged({{QStringList{inhibition.who, inhibition.why}}}, {});
    });

    return cookie;
}

uint PolicyAgent::AddInhibition(uint types, const QString &appName, const QString &reason)
{
    if (calledFromDBus()) {
        return addInhibitionWithExplicitDBusService(types, appName, reason, message().service());
    } else {
        return addInhibitionWithExplicitDBusService(types, appName, reason, QString());
    }
}

void PolicyAgent::addInhibitionTypeHelper(uint cookie, PolicyAgent::RequiredPolicies types)
{
    // Look through all of the inhibition types
    bool notify = false;
    if (types & ChangeScreenSettings) {
        // Check if we have to notify
        qCDebug(POWERDEVIL) << "Added change screen settings";
        if (m_typesToCookie[ChangeScreenSettings].isEmpty()) {
            notify = true;
        }
        m_typesToCookie[ChangeScreenSettings].append(cookie);
        types |= InterruptSession; // implied by ChangeScreenSettings
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
        // emit the signal - inhibition has changed
        Q_EMIT unavailablePoliciesChanged(unavailablePolicies());
    }
}

void PolicyAgent::ReleaseInhibition(uint cookie, bool retainCookie)
{
    qCDebug(POWERDEVIL) << "Releasing inhibition with cookie" << cookie;

    QString service = m_cookieToBusService.take(cookie);
    if (!m_busWatcher.isNull() && !service.isEmpty() && !m_cookieToBusService.key(service)) {
        // no cookies from service left
        m_busWatcher.data()->removeWatchedService(service);
    }

    if (m_pendingInhibitions.remove(cookie)) {
        qCDebug(POWERDEVIL) << "Inhibition was only scheduled but not enforced yet, just discarding it";
        return;
    }
    bool wasActive = m_activeInhibitions.remove(cookie);

    // Delete information about the inhibition,
    // except when it is just being suppressed, we may need to restore it later
    InhibitionInternals inhibition = m_cookieToInhibition.value(cookie);
    if (!retainCookie) {
        m_requestedInhibitions.removeOne(cookie);
        m_cookieToInhibition.remove(cookie);

        if (m_inhibitionAllowedChangedConnections.contains(cookie)) {
            disconnect(m_inhibitionAllowedChangedConnections.take(cookie));
        }
    }

    if (!wasActive && retainCookie) {
        qCDebug(POWERDEVIL) << "Inhibition was already suppressed, but retaining cookie";
        return;
    } else if (!wasActive && !retainCookie) {
        qCDebug(POWERDEVIL) << "Inhibition was already suppressed, just discarding it";
        Q_EMIT dbusPropertiesChanged({u"RequestedInhibitions"_s});
        return;
    }
    Q_EMIT dbusPropertiesChanged({u"RequestedInhibitions"_s, u"ActiveInhibitions"_s});
    Q_EMIT InhibitionsChanged(QList<QStringList>(), {{inhibition.who}});

    // Look through all of the inhibition types
    bool notify = false;
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
        Q_EMIT unavailablePoliciesChanged(unavailablePolicies());
    }
}

QList<QStringList> PolicyAgent::ListInhibitions() const
{
    QList<QStringList> result;
    std::ranges::transform(listActiveInhibitions(), std::back_inserter(result), [this](const PolicyAgentInhibition &inh) {
        return QStringList{inh.who, inh.why};
    });
    return result;
}

QList<PolicyAgentInhibition> PolicyAgent::listActiveInhibitions() const
{
    QList<PolicyAgentInhibition> result = listRequestedInhibitions();
    erase_if(result, [this](const PolicyAgentInhibition &inh) {
        return !(inh.flags & PolicyAgentInhibition::Active);
    });
    return result;
}

QList<PolicyAgentInhibition> PolicyAgent::listRequestedInhibitions() const
{
    QList<PolicyAgentInhibition> result;
    std::ranges::transform(m_requestedInhibitions, std::back_inserter(result), [this](uint cookie) {
        InhibitionInternals inhibition = m_cookieToInhibition.value(cookie);
        PolicyAgentInhibition::Flags flags{};
        if (!m_configuredToBlockInhibitions.contains(qMakePair(inhibition.who, inhibition.why))) {
            // in the future, an inhibition may be tagged inactive despite being allowed (for other reasons)
            flags |= PolicyAgentInhibition::Allowed;
            flags |= PolicyAgentInhibition::Active;
        }
        return PolicyAgentInhibition{
            .what = logindWhatForPolicies(inhibition.policies),
            .who = inhibition.who,
            .why = inhibition.why,
            .mode = u"block"_s, // a "block"-type inhibition, not an inhibition that was suppressed
                                // (this is why we avoid using "block" in the API in favor of "allow")
            .flags = flags,
        };
    });
    return result;
}

bool PolicyAgent::HasInhibition(/*PolicyAgent::RequiredPolicies*/ uint types)
{
    return requirePolicyCheck(static_cast<PolicyAgent::RequiredPolicies>(types)) != PolicyAgent::None;
}

void PolicyAgent::releaseAllInhibitions()
{
    const QList<uint> allCookies = m_cookieToInhibition.keys();
    for (uint cookie : allCookies) {
        ReleaseInhibition(cookie);
    }
}

void PolicyAgent::setupSystemdInhibition()
{
    if (m_systemdInhibitFd.fileDescriptor() != -1)
        return;

    if (!m_managerIface)
        return;

    // inhibit systemd/ConsoleKit2 handling of power/sleep/lid buttons
    // https://www.freedesktop.org/wiki/Software/systemd/inhibit
    // https://consolekit2.github.io/ConsoleKit2/#Manager.Inhibit
    qCDebug(POWERDEVIL) << "fd passing available:"
                        << bool(m_managerIface.data()->connection().connectionCapabilities() & QDBusConnection::UnixFileDescriptorPassing);

    QVariantList args;
    args << QStringLiteral("handle-power-key:handle-suspend-key:handle-hibernate-key:handle-lid-switch"); // what
    args << QStringLiteral("PowerDevil"); // who
    args << QStringLiteral("KDE handles power events"); // why
    args << QStringLiteral("block"); // mode
    QDBusPendingReply<QDBusUnixFileDescriptor> desc = m_managerIface.data()->asyncCallWithArgumentList(QStringLiteral("Inhibit"), args);
    desc.waitForFinished();
    if (desc.isValid()) {
        m_systemdInhibitFd = desc.value();
        qCDebug(POWERDEVIL) << "systemd powersave events handling inhibited, descriptor:" << m_systemdInhibitFd.fileDescriptor();
    } else
        qCWarning(POWERDEVIL) << "failed to inhibit systemd powersave handling";
}

void PolicyAgent::SetInhibitionAllowed(const QString &appName, const QString &reason, bool allowed)
{
    const RequestedInhibitionId id = qMakePair(appName, reason);

    if (allowed) {
        qCDebug(POWERDEVIL) << "Allowing (reactivating) inhibitions from" << appName << "with reason" << reason;

        if (!m_configuredToBlockInhibitions.contains(id)) {
            qCDebug(POWERDEVIL) << "Inhibitions are already active, ignoring call.";
            return;
        }
        m_configuredToBlockInhibitions.remove(id);
    } else {
        qCDebug(POWERDEVIL) << "Blocking inhibitions from" << appName << "with reason" << reason;

        if (m_configuredToBlockInhibitions.contains(id)) {
            qCDebug(POWERDEVIL) << "Inhibitions are already blocked, ignoring call.";
            return;
        }
        m_configuredToBlockInhibitions.insert(id);
    }

    Q_EMIT inhibitionAllowedChanged(id, allowed);

    QStringList configuredBlockedInhibitions;
    std::ranges::transform(std::as_const(m_configuredToBlockInhibitions),
                           std::back_inserter(configuredBlockedInhibitions),
                           [](const RequestedInhibitionId &id) {
                               return id.first + QLatin1Char(':') + id.second;
                           });
    m_config->setBlockedInhibitions(configuredBlockedInhibitions);
    m_config->save();
}
} // namespace PowerDevil

#include "moc_powerdevilpolicyagent.cpp"
