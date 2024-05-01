/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2012 Lukáš Tinkl <ltinkl@redhat.com>
 *   SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>

#include <KSharedConfig>
#include <qobjectdefs.h>

#include "PowerDevilGlobalSettings.h"
#include "powerdevilcore_export.h"

class QDBusServiceWatcher;
class QDBusInterface;

class OrgFreedesktopScreenSaverInterface;

inline constexpr QLatin1StringView SYSTEMD_LOGIN1_SERVICE("org.freedesktop.login1");
inline constexpr QLatin1StringView SYSTEMD_LOGIN1_PATH("/org/freedesktop/login1");
inline constexpr QLatin1StringView SYSTEMD_LOGIN1_MANAGER_IFACE("org.freedesktop.login1.Manager");
inline constexpr QLatin1StringView SYSTEMD_LOGIN1_SESSION_IFACE("org.freedesktop.login1.Session");
inline constexpr QLatin1StringView SYSTEMD_LOGIN1_SEAT_IFACE("org.freedesktop.login1.Seat");

inline constexpr QLatin1StringView CONSOLEKIT_SERVICE("org.freedesktop.ConsoleKit");
inline constexpr QLatin1StringView CONSOLEKIT_MANAGER_PATH("/org/freedesktop/ConsoleKit/Manager");
inline constexpr QLatin1StringView CONSOLEKIT_MANAGER_IFACE("org.freedesktop.ConsoleKit.Manager");

using InhibitionInfo = QPair<QString, QString>;

struct LogindInhibition {
    QString what;
    QString who;
    QString why;
    QString mode;
    uint pid;
    uint uid;

    bool operator==(const LogindInhibition &other) const
    {
        return what == other.what && who == other.who && why == other.why && mode == other.mode && pid == other.pid && uid == other.uid;
    }
};

namespace PowerDevil
{
class GlobalSettings;
class POWERDEVILCORE_EXPORT PolicyAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(PolicyAgent)

    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.PolicyAgent")

public:
    enum RequiredPolicy {
        None = 0,
        InterruptSession = 1,
        ChangeScreenSettings = 4,
    };
    Q_DECLARE_FLAGS(RequiredPolicies, RequiredPolicy)

    static PolicyAgent *instance();

    ~PolicyAgent() override;

    /**
     * This function performs a policy check on given policies, and returns a set of unsatisfiable policies,
     * or \c None if all the policies are satisfiable and the action can be carried on.
     */
    RequiredPolicies requirePolicyCheck(RequiredPolicies policies);

    RequiredPolicies unavailablePolicies();

    bool screenLockerActive() const;

    void setupSystemdInhibition();

public Q_SLOTS:
    // Exported slots
    uint AddInhibition(uint types, const QString &appName, const QString &reason);
    void ReleaseInhibition(uint cookie, bool retainCookie = false);
    QList<InhibitionInfo> ListInhibitions() const;
    bool HasInhibition(uint types);

    void releaseAllInhibitions();

    void BlockInhibition(const QString &appName, const QString &reason, bool permanently);
    void UnblockInhibition(const QString &appName, const QString &reason, bool permanently);
    QList<InhibitionInfo> ListBlockedInhibitions() const;

Q_SIGNALS:
    // Exported signals
    void InhibitionsChanged(const QList<InhibitionInfo> &added, const QStringList &removed);
    void BlockedInhibitionsChanged(const QList<InhibitionInfo> &added, const QList<InhibitionInfo> &removed);

    void unavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies newpolicies);
    void sessionActiveChanged(bool active);
    void screenLockerActiveChanged(bool active);

private Q_SLOTS:
    void onServiceUnregistered(const QString &serviceName);
    void onSessionHandlerRegistered(const QString &serviceName);
    void onSessionHandlerUnregistered(const QString &serviceName);
    void onActiveSessionChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);
    void onActiveSessionChanged(const QString &activeSession);

    void onManagerPropertyChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);

private:
    explicit PolicyAgent(QObject *parent = nullptr);

    void init(GlobalSettings *globalSettings);

    void addInhibitionTypeHelper(uint cookie, RequiredPolicies types);

    void checkLogindInhibitions();

    // Screen locker integration
    void onScreenLockerOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    QDBusServiceWatcher *m_screenLockerWatcher;

    void onScreenLockerActiveChanged(bool active);
    OrgFreedesktopScreenSaverInterface *m_screenLockerInterface = nullptr;
    bool m_screenLockerActive = false;

    // This function serves solely for fd.o connector
    uint addInhibitionWithExplicitDBusService(uint types, const QString &appName, const QString &reason, const QString &service);

    // used by systemd and ConsoleKit
    QScopedPointer<QDBusInterface> m_managerIface;

    // systemd support
    QString getNamedPathProperty(const QString &path, const QString &iface, const QString &prop) const;
    bool m_sdAvailable;
    QString m_activeSessionPath;
    QPointer<QDBusInterface> m_sdSessionInterface;
    QPointer<QDBusInterface> m_sdSeatInterface;
    QDBusUnixFileDescriptor m_systemdInhibitFd;

    // ConsoleKit support
    bool m_ckAvailable;
    QPointer<QDBusInterface> m_ckSessionInterface;
    QPointer<QDBusInterface> m_ckSeatInterface;
    bool m_sessionIsBeingInterrupted;

    QHash<uint, QPair<QString, QString>> m_cookieToAppName;
    QHash<uint, QString> m_cookieToBusService;
    QHash<RequiredPolicy, QList<uint>> m_typesToCookie;

    QHash<uint, LogindInhibition> m_logindInhibitions;

    QList<int> m_pendingInhibitions;
    QSet<uint> m_activeInhibitions;
    QSet<uint> m_blockedInhibitions;
    QSet<uint> m_pendingBlockedInhibitions;

    uint m_lastCookie;

    QPointer<QDBusServiceWatcher> m_busWatcher;
    QPointer<QDBusServiceWatcher> m_sdWatcher;
    QPointer<QDBusServiceWatcher> m_ckWatcher;

    bool m_wasLastActiveSession;

    GlobalSettings *m_config = nullptr;
    QSet<InhibitionInfo> m_configuredToBlockInhibitions;

    QHash<uint, QMetaObject::Connection> m_blockInhibitionConnections;
    QHash<uint, QMetaObject::Connection> m_unblockInhibitionConnections;

    friend class Core;
    friend class FdoConnector;

Q_SIGNALS:
    void blockInhibitionRequested(InhibitionInfo info);
    void unblockInhibitionRequested(InhibitionInfo info);
};
}
