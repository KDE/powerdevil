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


#ifndef POWERDEVILCORE_H
#define POWERDEVILCORE_H

#include "powerdevilbackendinterface.h"

#include <QtCore/QWeakPointer>
#include <QtCore/QStringList>

#include <KSharedConfig>

namespace KActivities
{
    class Consumer;
} // namespace KActivities
typedef QMap< QString, QString > StringStringMap;

class KDirWatch;
class QDBusServiceWatcher;
class QTimer;
class KNotification;

namespace Solid {
class Battery;
}

namespace PowerDevil
{

class BackendInterface;
class Action;

class Q_DECL_EXPORT Core : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")

public:
    explicit Core(QObject* parent);
    virtual ~Core();

    void reloadProfile(int state);

    void emitNotification(const QString &evid, const QString &message = QString(),
                          const QString &iconname = QString());
    void emitRichNotification(const QString &evid, const QString &title, const QString &message = QString());
    bool emitBatteryChargePercentNotification(int currentPercent, int previousPercent);

    BackendInterface *backend();

    // More...

public Q_SLOTS:
    void loadCore(PowerDevil::BackendInterface *backend);
    // Set of common action - useful for the DBus interface
    uint backendCapabilities();
    void refreshStatus();
    void reparseConfiguration();

    QString checkBatteryStatus(bool notify = true);

    QString currentProfile() const;
    void loadProfile(bool force = false);

    qulonglong batteryRemainingTime() const;

    bool isLidClosed() const;
    bool isLidPresent() const;
    bool isActionSupported(const QString &actionName);

Q_SIGNALS:
    void coreReady();
    void profileChanged(const QString &newProfile);
    void configurationReloaded();
    void batteryRemainingTimeChanged(qulonglong time);

private:
    void registerActionTimeout(Action *action, int timeout);
    void unregisterActionTimeouts(Action *action);

    /**
     * Computes the current global charge percentage.
     * Sum of all battery charges.
     */
    int currentChargePercent() const;

    friend class Action;

    BackendInterface *m_backend;
    QStringList m_loadedBatteriesUdi;

    QWeakPointer< KNotification > notification;
    QDBusServiceWatcher *m_notificationsWatcher;

    KSharedConfigPtr m_profilesConfig;

    QString m_currentProfile;

    QHash< QString, int > m_batteriesPercent;
    QHash< QString, bool > m_batteriesCharged;

    QTimer *m_criticalBatteryTimer;

    KActivities::Consumer *m_activityConsumer;

    // Idle time management
    QHash< Action*, QList< int > > m_registeredActionTimeouts;
    QList< Action* > m_pendingResumeFromIdleActions;
    bool m_pendingWakeupEvent;

    // Activity inhibition management
    QHash< QString, int > m_sessionActivityInhibit;
    QHash< QString, int > m_screenActivityInhibit;

private Q_SLOTS:
    void onBackendReady();
    void onBackendError(const QString &error);
    void onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState);
    void onBatteryChargePercentChanged(int,const QString&);
    void onBatteryChargeStateChanged(int,const QString&);
    void onBatteryRemainingTimeChanged(qulonglong);
    void onKIdleTimeoutReached(int,int);
    void onResumingFromIdle();
    void onDeviceAdded(const QString &udi);
    void onDeviceRemoved(const QString &udi);
    void onCriticalBatteryTimerExpired();
    void onNotificationTimeout();
    void onServiceRegistered(const QString &service);
};

}

Q_DECLARE_METATYPE(StringStringMap)

#endif // POWERDEVILCORE_H
