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

static constexpr QLatin1StringView SOLID_POWERMANAGEMENT_SERVICE("org.kde.Solid.PowerManagement");
static constexpr QLatin1StringView FDO_POWERMANAGEMENT_SERVICE("org.freedesktop.PowerManagement");

InhibitionControl::InhibitionControl(QObject *parent)
    : QObject(parent)
    , m_solidWatcher(new QDBusServiceWatcher)
    , m_fdoWatcher(new QDBusServiceWatcher)
{
    qDBusRegisterMetaType<QList<InhibitionInfo>>();
    qDBusRegisterMetaType<InhibitionInfo>();

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

        onInhibitionsChanged({}, {});
        onPermanentlyBlockedInhibitionsChanged({}, {});
        onTemporarilyBlockedInhibitionsChanged({}, {});

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
                                                   QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                   QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                   QStringLiteral("InhibitionsChanged"),
                                                   this,
                                                   SLOT(onInhibitionsChanged(QList<InhibitionInfo>, QStringList)))) {
            qCDebug(APPLETS::BATTERYMONITOR) << "Error connecting to inhibition changes via dbus";
        }
        if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                                   QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                   QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                   QStringLiteral("PermanentlyBlockedInhibitionsChanged"),
                                                   this,
                                                   SLOT(onPermanentlyBlockedInhibitionsChanged(QList<InhibitionInfo>, QList<InhibitionInfo>)))) {
            qCDebug(APPLETS::BATTERYMONITOR) << "Error connecting to permanently blocked inhibition changes via dbus";
        }
        if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                                   QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                   QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                   QStringLiteral("TemporarilyBlockedInhibitionsChanged"),
                                                   this,
                                                   SLOT(onTemporarilyBlockedInhibitionsChanged(QList<InhibitionInfo>, QList<InhibitionInfo>)))) {
            qCDebug(APPLETS::BATTERYMONITOR) << "Error connecting to temporarily blocked inhibition changes via dbus";
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
                                                 QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                 QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                 QStringLiteral("InhibitionsChanged"),
                                                 this,
                                                 SLOT(onInhibitionsChanged(QList<InhibitionInfo>, QStringList)));
        QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                                 QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                 QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                 QStringLiteral("PermanentlyBlockedInhibitionsChanged"),
                                                 this,
                                                 SLOT(onPermanentlyBlockedInhibitionsChanged(QList<InhibitionInfo>, QList<InhibitionInfo>)));
        QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                                 QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                 QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                 QStringLiteral("TemporarilyBlockedInhibitionsChanged"),
                                                 this,
                                                 SLOT(onTemporarilyBlockedInhibitionsChanged(QList<InhibitionInfo>, QList<InhibitionInfo>)));

        m_inhibitions = QList<QVariantMap>();
        m_blockedInhibitions = QList<QVariantMap>();
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

void InhibitionControl::blockInhibition(const QString &appName, const QString &reason, bool permanently)
{
    qDebug() << "Blocking inhibition for" << appName << "with reason" << reason << (permanently ? "permanently" : "temporarily");
    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                      QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                      QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                      QStringLiteral("BlockInhibition"));
    msg << appName << reason << permanently;
    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
}

void InhibitionControl::unblockInhibition(const QString &appName, const QString &reason, bool permanently)
{
    qDebug() << "Unblocking inhibition for" << appName << "with reason" << reason << (permanently ? "permanently" : "temporarily");
    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                      QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                      QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                      QStringLiteral("UnblockInhibition"));
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

QBindable<QList<QVariantMap>> InhibitionControl::bindableBlockedInhibitions()
{
    return &m_blockedInhibitions;
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

void InhibitionControl::onInhibitionsChanged(const QList<InhibitionInfo> &added, const QStringList &removed)
{
    Q_UNUSED(added);
    Q_UNUSED(removed);

    QDBusMessage inhibitionsMessage = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                                     QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                                     QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                                     QStringLiteral("ListInhibitions"));
    QDBusPendingCall inhibitionsCall = QDBusConnection::sessionBus().asyncCall(inhibitionsMessage);
    auto inhibitionsWatcher = new QDBusPendingCallWatcher(inhibitionsCall, this);
    connect(inhibitionsWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusReply<QList<InhibitionInfo>> reply = *watcher;
        if (reply.isValid()) {
            updateInhibitions(reply.value());
        } else {
            qCDebug(APPLETS::BATTERYMONITOR) << "Failed to retrive inhibitions";
        }
        watcher->deleteLater();
    });
}

void InhibitionControl::onPermanentlyBlockedInhibitionsChanged(const QList<InhibitionInfo> &added, const QList<InhibitionInfo> &removed)
{
    updateBlockedInhibitions(added, removed, {}, {});
}

void InhibitionControl::onTemporarilyBlockedInhibitionsChanged(const QList<InhibitionInfo> &added, const QList<InhibitionInfo> &removed)
{
    updateBlockedInhibitions({}, {}, added, removed);
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

void InhibitionControl::updateInhibitions(const QList<InhibitionInfo> &inhibitions)
{
    QList<QVariantMap> out;

    for (auto it = inhibitions.constBegin(); it != inhibitions.constEnd(); ++it) {
        const QString &name = (*it).first;
        if (name == QStringLiteral("plasmashell") || name == QStringLiteral("org.kde.plasmashell")) {
            continue;
        }
        QString prettyName;
        QString icon;
        const QString &reason = (*it).second;

        m_data.populateApplicationData(name, &prettyName, &icon);

        out.append(QVariantMap{{QStringLiteral("Name"), name},
                               {QStringLiteral("PrettyName"), prettyName},
                               {QStringLiteral("Icon"), icon},
                               {QStringLiteral("Reason"), reason}});
    }

    m_inhibitions = out;
}

void InhibitionControl::updateBlockedInhibitions(const QList<InhibitionInfo> &permanentlyBlockedAdded,
                                                 const QList<InhibitionInfo> &permanentlyBlockedRemoved,
                                                 const QList<InhibitionInfo> &temporarilyBlockedAdded,
                                                 const QList<InhibitionInfo> &temporarilyBlockedRemoved)
{
    auto blockedInhibition = [this](const InhibitionInfo &item, const bool permanently) {
        const QString &name = item.first;
        QString prettyName;
        QString icon;
        const QString &reason = item.second;

        m_data.populateApplicationData(name, &prettyName, &icon);

        return QVariantMap{{QStringLiteral("Name"), name},
                           {QStringLiteral("PrettyName"), prettyName},
                           {QStringLiteral("Icon"), icon},
                           {QStringLiteral("Reason"), reason},
                           {QStringLiteral("Permanently"), permanently}};
    };

    auto out = InhibitionControl::bindableBlockedInhibitions().value();

    for (auto it = permanentlyBlockedAdded.constBegin(); it != permanentlyBlockedAdded.constEnd(); ++it) {
        out.append(blockedInhibition(*it, true));
    }
    for (auto it = permanentlyBlockedRemoved.constBegin(); it != permanentlyBlockedRemoved.constEnd(); ++it) {
        out.removeOne(blockedInhibition(*it, true));
    }
    for (auto it = temporarilyBlockedAdded.constBegin(); it != temporarilyBlockedAdded.constEnd(); ++it) {
        out.append(blockedInhibition(*it, false));
    }
    for (auto it = temporarilyBlockedRemoved.constBegin(); it != temporarilyBlockedRemoved.constEnd(); ++it) {
        out.removeOne(blockedInhibition(*it, false));
    }

    m_blockedInhibitions = out;
}

#include "moc_inhibitioncontrol.cpp"
