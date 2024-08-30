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

struct PolicyAgentInhibition {
    enum Flag {
        Active = 0x1,
        Allowed = 0x2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QString what;
    QString who;
    QString why;
    QString mode;
    uint flags;
};

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
    Q_PROPERTY(QList<PolicyAgentInhibition> RequestedInhibitions READ listRequestedInhibitions)
    Q_PROPERTY(QList<PolicyAgentInhibition> ActiveInhibitions READ listActiveInhibitions)

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

    // DEPRECATED: D-Bus getter method, returns array of {who, why}. Use properties instead
    QList<QStringList> ListInhibitions() const;

public Q_SLOTS:
    // Exported slots
    uint AddInhibition(uint types, const QString &who, const QString &why);
    void ReleaseInhibition(uint cookie, bool retainCookie = false);
    bool HasInhibition(uint types);

    void releaseAllInhibitions();

    void SetInhibitionAllowed(const QString &who, const QString &why, bool allowed);

Q_SIGNALS:
    // Exported signals
    void dbusPropertiesChanged(const QStringList &props);
    void InhibitionsChanged(const QList<QStringList> &added, const QStringList &removed); // DEPRECATED: array of {who, why} in `added`

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
    using RequestedInhibitionId = QPair<QString, QString>; // {who, why}

    struct InhibitionInternals {
        RequiredPolicies policies;
        QString who;
        QString why;
    };

    explicit PolicyAgent(QObject *parent = nullptr);

    void init(GlobalSettings *globalSettings);

    void addInhibitionTypeHelper(uint cookie, RequiredPolicies types);

    RequiredPolicies policiesInLogindWhat(const QString &what) const;
    QString logindWhatForPolicies(RequiredPolicies policies) const;
    void checkLogindInhibitions();

    // Screen locker integration
    void onScreenLockerActiveChanged(bool active);
    OrgFreedesktopScreenSaverInterface *m_screenLockerInterface = nullptr;
    bool m_screenLockerActive = false;

    // This function serves solely for fd.o connector
    uint addInhibitionWithExplicitDBusService(uint types, const QString &who, const QString &why, const QString &service);

    // D-Bus property getters
    QList<PolicyAgentInhibition> listActiveInhibitions() const;
    QList<PolicyAgentInhibition> listRequestedInhibitions() const;

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

    QHash<uint, InhibitionInternals> m_cookieToInhibition;
    QHash<uint, QString> m_cookieToBusService;
    QHash<RequiredPolicy, QList<uint>> m_typesToCookie;

    QHash<uint, LogindInhibition> m_logindInhibitions;

    QSet<uint> m_pendingInhibitions;
    QSet<uint> m_activeInhibitions;
    QList<uint> m_requestedInhibitions;

    uint m_lastCookie;

    QPointer<QDBusServiceWatcher> m_busWatcher;
    QPointer<QDBusServiceWatcher> m_sdWatcher;
    QPointer<QDBusServiceWatcher> m_ckWatcher;

    bool m_wasLastActiveSession;

    GlobalSettings *m_config = nullptr;
    QSet<RequestedInhibitionId> m_configuredToBlockInhibitions;

    QHash<uint, QMetaObject::Connection> m_inhibitionAllowedChangedConnections;

    friend class Core;
    friend class FdoConnector;

Q_SIGNALS:
    void inhibitionAllowedChanged(const RequestedInhibitionId &requested, bool allowed);
};
}
