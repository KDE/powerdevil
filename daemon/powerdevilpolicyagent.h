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


#ifndef POWERDEVIL_POWERDEVILPOLICYAGENT_H
#define POWERDEVIL_POWERDEVILPOLICYAGENT_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QWeakPointer>

#include <QtDBus/QDBusContext>

#include <kdemacros.h>

class QDBusServiceWatcher;
class QDBusInterface;

#define SYSTEMD_LOGIN1_SERVICE "org.freedesktop.login1"
#define SYSTEMD_LOGIN1_PATH "/org/freedesktop/login1"
#define SYSTEMD_LOGIN1_MANAGER_IFACE "org.freedesktop.login1.Manager"
#define SYSTEMD_LOGIN1_SESSION_IFACE "org.freedesktop.login1.Session"
#define SYSTEMD_LOGIN1_SEAT_IFACE "org.freedesktop.login1.Seat"

#define CONSOLEKIT_SERVICE "org.freedesktop.ConsoleKit"


namespace PowerDevil
{

class KDE_EXPORT PolicyAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(PolicyAgent)

    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.PolicyAgent")

public:
    enum RequiredPolicy {
        None = 0,
        InterruptSession = 1,
        ChangeProfile = 2,
        ChangeScreenSettings = 4
    };
    Q_DECLARE_FLAGS(RequiredPolicies, RequiredPolicy)

    static PolicyAgent *instance();

    virtual ~PolicyAgent();

    /**
     * This function performs a policy check on given policies, and returns a set of unsatisfiable policies,
     * or \c None if all the policies are satisfiable and the action can be carried on.
     */
    RequiredPolicies requirePolicyCheck(RequiredPolicies policies);

    RequiredPolicies unavailablePolicies();

public Q_SLOTS:
    // Exported slots
    uint AddInhibition(uint types, const QString &appName, const QString &reason);
    void ReleaseInhibition(uint cookie);

    void releaseAllInhibitions();
Q_SIGNALS:
    void unavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies newpolicies);

private Q_SLOTS:
    void onServiceUnregistered(const QString & serviceName);
    void onSessionHandlerRegistered(const QString & serviceName);
    void onSessionHandlerUnregistered(const QString & serviceName);
    void onActiveSessionChanged(const QString & ifaceName, const QVariantMap & changedProps, const QStringList & invalidatedProps);
    void onActiveSessionChanged(const QString &activeSession);

private:
    explicit PolicyAgent(QObject* parent = 0);

    void init();
    void startSessionInterruption();
    void finishSessionInterruption();

    void addInhibitionTypeHelper(uint cookie, RequiredPolicies types);

    // This function serves solely for fd.o connector
    uint addInhibitionWithExplicitDBusService(uint types, const QString &appName,
                                              const QString &reason, const QString &service);

    // systemd support
    QString getNamedPathProperty(const QString & path, const QString & iface, const QString & prop) const;
    bool m_sdAvailable;
    QString m_activeSessionPath;
    QWeakPointer< QDBusInterface > m_sdSessionInterface;
    QWeakPointer< QDBusInterface > m_sdSeatInterface;

    // ConsoleKit support
    bool m_ckAvailable;
    QWeakPointer< QDBusInterface > m_ckSessionInterface;
    QWeakPointer< QDBusInterface > m_ckSeatInterface;
    bool m_sessionIsBeingInterrupted;

    QHash< uint, QPair< QString, QString > > m_cookieToAppName;
    QHash< uint, QString > m_cookieToBusService;
    QHash< RequiredPolicy, QList< uint > > m_typesToCookie;

    uint m_lastCookie;

    QWeakPointer< QDBusServiceWatcher > m_busWatcher;
    QWeakPointer< QDBusServiceWatcher > m_sdWatcher;
    QWeakPointer< QDBusServiceWatcher > m_ckWatcher;

    bool m_wasLastActiveSession;

    friend class Core;
    friend class FdoConnector;
};

}

#endif // POWERDEVIL_POWERDEVILPOLICYAGENT_H
