/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 * SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>

 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "inhibitioncontrol.h"

#include "batterymonitor_debug.h"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QString>
#include <QVariant>

#include <KService>

#include "inhibitmonitor_p.h"

using namespace Qt::Literals::StringLiterals;

static constexpr QLatin1StringView SOLID_POWERMANAGEMENT_SERVICE("org.kde.Solid.PowerManagement");
static constexpr QLatin1StringView FDO_POWERMANAGEMENT_SERVICE("org.freedesktop.PowerManagement");

static const QString POLICY_AGENT_PATH = u"/org/kde/Solid/PowerManagement/PolicyAgent"_s;
static const QString POLICY_AGENT_IFACE = u"org.kde.Solid.PowerManagement.PolicyAgent"_s;

Q_DECLARE_METATYPE(PolicyAgentInhibition)
Q_DECLARE_METATYPE(QList<PolicyAgentInhibition>)

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

InhibitionControl::InhibitionControl(QObject *parent)
    : QObject(parent)
    , m_solidWatcher(new QDBusServiceWatcher)
    , m_fdoWatcher(new QDBusServiceWatcher)
{
    qDBusRegisterMetaType<PolicyAgentInhibition>();
    qDBusRegisterMetaType<QList<PolicyAgentInhibition>>();

    // Watch for PowerDevil's power management service
    m_solidWatcher->setConnection(QDBusConnection::sessionBus());
    m_solidWatcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    m_solidWatcher->addWatchedService(SOLID_POWERMANAGEMENT_SERVICE);

    connect(m_solidWatcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &InhibitionControl::onServiceRegistered);
    connect(m_solidWatcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &InhibitionControl::onServiceUnregistered);
    // If it's up and running already, let's cache it
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(SOLID_POWERMANAGEMENT_SERVICE)) {
        onServiceRegistered(SOLID_POWERMANAGEMENT_SERVICE);
    }

    // Watch for the freedesktop.org power management service
    m_fdoWatcher->setConnection(QDBusConnection::sessionBus());
    m_fdoWatcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    m_fdoWatcher->addWatchedService(FDO_POWERMANAGEMENT_SERVICE);

    connect(m_fdoWatcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &InhibitionControl::onServiceRegistered);
    connect(m_fdoWatcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &InhibitionControl::onServiceUnregistered);
    // If it's up and running already, let's cache it
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(FDO_POWERMANAGEMENT_SERVICE)) {
        onServiceRegistered(FDO_POWERMANAGEMENT_SERVICE);
    }
}

InhibitionControl::~InhibitionControl()
{
}

void InhibitionControl::onServiceRegistered(const QString &serviceName)
{
    if (serviceName == FDO_POWERMANAGEMENT_SERVICE) {
        if (!QDBusConnection::sessionBus().connect(FDO_POWERMANAGEMENT_SERVICE,
                                                   QStringLiteral("/org/freedesktop/PowerManagement"),
                                                   QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                                                   QStringLiteral("HasInhibitChanged"),
                                                   this,
                                                   SLOT(onHasInhibitionChanged(bool)))) {
            qCDebug(APPLETS::BATTERYMONITOR) << "Error connecting to fdo inhibition changes via dbus";
        }
    } else if (serviceName == SOLID_POWERMANAGEMENT_SERVICE) {
        m_isManuallyInhibited = InhibitMonitor::self().getInhibit();
        connect(&InhibitMonitor::self(), &InhibitMonitor::isManuallyInhibitedChanged, this, &InhibitionControl::onIsManuallyInhibitedChanged);
        connect(&InhibitMonitor::self(), &InhibitMonitor::isManuallyInhibitedChangeError, this, &InhibitionControl::onisManuallyInhibitedErrorChanged);

        QDBusMessage isLidPresentMessage = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                                          QStringLiteral("/org/kde/Solid/PowerManagement"),
                                                                          SOLID_POWERMANAGEMENT_SERVICE,
                                                                          QStringLiteral("isLidPresent"));
        QDBusPendingCall isLidPresentCall = QDBusConnection::sessionBus().asyncCall(isLidPresentMessage);
        auto isLidPresentWatcher = new QDBusPendingCallWatcher(isLidPresentCall, this);
        connect(isLidPresentWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            QDBusReply<bool> reply = *watcher;
            if (reply.isValid()) {
                m_isLidPresent = reply.value();

                QDBusMessage triggersLidActionMessage =
                    QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                   QStringLiteral("/org/kde/Solid/PowerManagement/Actions/HandleButtonEvents"),
                                                   QStringLiteral("org.kde.Solid.PowerManagement.Actions.HandleButtonEvents"),
                                                   QStringLiteral("triggersLidAction"));
                QDBusPendingCall triggersLidActionCall = QDBusConnection::sessionBus().asyncCall(triggersLidActionMessage);
                auto triggersLidActionWatcher = new QDBusPendingCallWatcher(triggersLidActionCall, this);
                connect(triggersLidActionWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                    QDBusReply<bool> reply = *watcher;
                    if (reply.isValid()) {
                        m_triggersLidAction = reply.value();
                    }
                    watcher->deleteLater();
                });
                if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                                           QStringLiteral("/org/kde/Solid/PowerManagement/Actions/HandleButtonEvents"),
                                                           QStringLiteral("org.kde.Solid.PowerManagement.Actions.HandleButtonEvents"),
                                                           QStringLiteral("triggersLidActionChanged"),
                                                           this,
                                                           SLOT(triggersLidActionChanged(bool)))) {
                    qCDebug(APPLETS::BATTERYMONITOR) << "error connecting to lid action trigger changes via dbus";
                }
            } else {
                qCDebug(APPLETS::BATTERYMONITOR) << "Lid is not present";
            }
            watcher->deleteLater();
        });

        checkInhibitions();

        QDBusMessage hasInhibitMessage = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                                        QStringLiteral("/org/freedesktop/PowerManagement"),
                                                                        QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                                                                        QStringLiteral("HasInhibit"));
        QDBusPendingCall hasInhibitReply = QDBusConnection::sessionBus().asyncCall(hasInhibitMessage);
        auto *hasInhibitWatcher = new QDBusPendingCallWatcher(hasInhibitReply, this);
        connect(hasInhibitWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            QDBusReply<bool> reply = *watcher;
            if (reply.isValid()) {
                m_hasInhibition = reply.value();
            } else {
                qCDebug(APPLETS::BATTERYMONITOR) << "Failed to retrive has inhibit";
            }
            watcher->deleteLater();
        });

        if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                                   POLICY_AGENT_PATH,
                                                   u"org.freedesktop.DBus.Properties"_s,
                                                   u"PropertiesChanged"_s,
                                                   this,
                                                   SLOT(onPolicyAgentPropertiesChanged(QString, QVariantMap, QStringList)))) {
            qCDebug(APPLETS::BATTERYMONITOR) << "Error connecting to PolicyAgent inhibition changes via dbus";
        }
    }
}

void InhibitionControl::onServiceUnregistered(const QString &serviceName)
{
    if (serviceName == FDO_POWERMANAGEMENT_SERVICE) {
        QDBusConnection::sessionBus().disconnect(FDO_POWERMANAGEMENT_SERVICE,
                                                 QStringLiteral("/org/freedesktop/PowerManagement"),
                                                 QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                                                 QStringLiteral("HasInhibitChanged"),
                                                 this,
                                                 SLOT(onHasInhibitionChanged(bool)));
    } else if (serviceName == SOLID_POWERMANAGEMENT_SERVICE) {
        disconnect(&InhibitMonitor::self(), &InhibitMonitor::isManuallyInhibitedChanged, this, &InhibitionControl::onIsManuallyInhibitedChanged);
        disconnect(&InhibitMonitor::self(), &InhibitMonitor::isManuallyInhibitedChangeError, this, &InhibitionControl::onisManuallyInhibitedErrorChanged);

        QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                                 QStringLiteral("/org/kde/Solid/PowerManagement/Actions/HandleButtonEvents"),
                                                 QStringLiteral("org.kde.Solid.PowerManagement.Actions.HandleButtonEvents"),
                                                 QStringLiteral("triggersLidActionChanged"),
                                                 this,
                                                 SLOT(triggersLidActionChanged(bool)));
        QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                                 POLICY_AGENT_PATH,
                                                 POLICY_AGENT_IFACE,
                                                 QStringLiteral("InhibitionsChanged"),
                                                 this,
                                                 SLOT(onInhibitionsChanged(QList<InhibitionInfo>, QStringList)));
        QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                                 POLICY_AGENT_PATH,
                                                 u"org.freedesktop.DBus.Properties"_s,
                                                 u"PropertiesChanged"_s,
                                                 this,
                                                 SLOT(onPolicyAgentPropertiesChanged(QString, QVariantMap, QStringList)));

        m_inhibitions = QList<QVariantMap>();
        m_suppressedInhibitions = QList<QVariantMap>();
        m_hasInhibition = false;
        m_isManuallyInhibited = false;
        m_isManuallyInhibitedError = false;
        m_isLidPresent = false;
        m_triggersLidAction = false;
    }
}

void InhibitionControl::inhibit(const QString &reason)
{
    InhibitMonitor::self().inhibit(reason, m_isSilent);
}

void InhibitionControl::uninhibit()
{
    InhibitMonitor::self().uninhibit(m_isSilent);
}

void InhibitionControl::suppressInhibition(const QString &appName, const QString &reason, bool permanently)
{
    qCDebug(APPLETS::BATTERYMONITOR) << "Suppressing inhibition for" << appName << "with reason" << reason << (permanently ? "permanently" : "temporarily");
    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE, POLICY_AGENT_PATH, POLICY_AGENT_IFACE, u"SuppressInhibition"_s);
    msg << appName << reason << permanently;
    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
}

void InhibitionControl::unsuppressInhibition(const QString &appName, const QString &reason, bool permanently)
{
    qCDebug(APPLETS::BATTERYMONITOR) << "Unsuppressing inhibition for" << appName << "with reason" << reason << (permanently ? "permanently" : "temporarily");
    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE, POLICY_AGENT_PATH, POLICY_AGENT_IFACE, u"UnsuppressInhibition"_s);
    msg << appName << reason << permanently;
    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
}

bool InhibitionControl::isSilent()
{
    return m_isSilent;
}

void InhibitionControl::setIsSilent(bool status)
{
    m_isSilent = status;
}

QBindable<QList<QVariantMap>> InhibitionControl::bindableInhibitions()
{
    return &m_inhibitions;
}

QBindable<QList<QVariantMap>> InhibitionControl::bindableSuppressedInhibitions()
{
    return &m_suppressedInhibitions;
}

QBindable<bool> InhibitionControl::bindableHasInhibition()
{
    return &m_hasInhibition;
}

QBindable<bool> InhibitionControl::bindableIsLidPresent()
{
    return &m_isLidPresent;
}

QBindable<bool> InhibitionControl::bindableTriggersLidAction()
{
    return &m_triggersLidAction;
}

QBindable<bool> InhibitionControl::bindableIsManuallyInhibited()
{
    return &m_isManuallyInhibited;
}

QBindable<bool> InhibitionControl::bindableIsManuallyInhibitedError()
{
    return &m_isManuallyInhibitedError;
}

void InhibitionControl::onHasInhibitionChanged(bool status)
{
    m_hasInhibition = status;
}

void InhibitionControl::onIsManuallyInhibitedChanged(bool status)
{
    m_isManuallyInhibited = status;
}

void InhibitionControl::onisManuallyInhibitedErrorChanged(bool status)
{
    m_isManuallyInhibitedError = status;
}

void InhibitionControl::onPolicyAgentPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    const QString active = u"ActiveInhibitions"_s;
    const QString suppressed = u"SuppressedInhibitions"_s;

    if (ifaceName == POLICY_AGENT_IFACE) {
        if (changedProps.contains(active) || invalidatedProps.contains(active) || changedProps.contains(suppressed) || invalidatedProps.contains(suppressed)) {
            checkInhibitions();
        }
    }
}

void InhibitionControl::checkInhibitions()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE, POLICY_AGENT_PATH, u"org.freedesktop.DBus.Properties"_s, u"GetAll"_s);
    msg << POLICY_AGENT_IFACE;
    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);

    auto watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusReply<QVariantMap> reply = *watcher;
        if (reply.isValid()) {
            const QVariantMap &props = reply.value();
            m_inhibitions = updatedInhibitions(qdbus_cast<QList<PolicyAgentInhibition>>(props.value(u"ActiveInhibitions"_s).value<QDBusArgument>()));
            m_suppressedInhibitions =
                updatedInhibitions(qdbus_cast<QList<PolicyAgentInhibition>>(props.value(u"SuppressedInhibitions"_s).value<QDBusArgument>()));
        } else {
            qCDebug(APPLETS::BATTERYMONITOR) << "Failed to retrive inhibitions";
        }
        watcher->deleteLater();
    });
}

QList<QVariantMap> InhibitionControl::updatedInhibitions(const QList<PolicyAgentInhibition> &inhibitions)
{
    QList<QVariantMap> result;

    for (const PolicyAgentInhibition &inhibition : inhibitions) {
        if (inhibition.who == u"plasmashell"_s || inhibition.who == u"org.kde.plasmashell"_s) {
            continue;
        }
        if (inhibition.mode != u"block"_s) {
            continue;
        }
        QStringList whats = inhibition.what.split(":"_L1);
        if (!whats.contains(u"sleep"_s) && !whats.contains(u"idle"_s)) {
            continue; // maybe we'll support other inhibition types at a later point
        }
        QString prettyName;
        QString icon;
        m_data.populateApplicationData(inhibition.who, &prettyName, &icon);

        result.append(QVariantMap{
            {u"Name"_s, inhibition.who},
            {u"PrettyName"_s, prettyName},
            {u"Icon"_s, icon},
            {u"Reason"_s, inhibition.why},
            {u"ConfiguredToSuppress"_s, static_cast<bool>(inhibition.flags & PolicyAgentInhibition::ConfiguredToSuppress)},
        });
    }

    return result;
}

#include "moc_inhibitioncontrol.cpp"
