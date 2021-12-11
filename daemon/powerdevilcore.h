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

#include <QPointer>
#include <QSet>
#include <QStringList>

#include <QDBusMessage>
#include <QDBusError>
#include <QDBusContext>
#include <QDBusObjectPath>

#include <QSocketNotifier>

#include <KSharedConfig>

namespace KActivities
{
    class Consumer;
} // namespace KActivities

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

struct WakeupInfo {
    QString service;
    QDBusObjectPath path;
    int cookie;
    qint64 timeout;
};

class Q_DECL_EXPORT Core : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")

public:
    explicit Core(QObject* parent);
    ~Core() override;

    void reloadProfile(int state);

    void emitRichNotification(const QString &evid, const QString &title, const QString &message = QString());

    void emitNotification(const QString &eventId, const QString &title, const QString &message, const QString &iconName);

    bool emitBatteryChargePercentNotification(int currentPercent, int previousPercent, const QString &udi = QString());

    BackendInterface *backend();

    // More...

public Q_SLOTS:
    void loadCore(PowerDevil::BackendInterface *backend);
    // Set of common action - useful for the DBus interface
    uint backendCapabilities();
    void refreshStatus();
    void reparseConfiguration();

    QString currentProfile() const;
    void loadProfile(bool force = false);

    qulonglong batteryRemainingTime() const;

    bool isLidClosed() const;
    bool isLidPresent() const;
    bool isActionSupported(const QString &actionName);
    bool hasDualGpu() const;
    int chargeStartThreshold() const;
    int chargeStopThreshold() const;

    // service - dbus interface to ping when wakeup is done
    // path - dbus path on service
    // cookie - data to pass back
    // silent - true if silent wakeup is needed
    uint scheduleWakeup(const QString &service, const QDBusObjectPath &path, qint64 timeout);
    void wakeup();
    void clearWakeup(int cookie);

Q_SIGNALS:
    void coreReady();
    void profileChanged(const QString &newProfile);
    void configurationReloaded();
    void batteryRemainingTimeChanged(qulonglong time);
    void lidClosedChanged(bool closed);
    void chargeStartThresholdChanged(int threshold);
    void chargeStopThresholdChanged(int threshold);

private:
    void registerActionTimeout(Action *action, int timeout);
    void unregisterActionTimeouts(Action *action);
    void handleLowBattery(int percent);
    void handleCriticalBattery(int percent);

    void readChargeThreshold();

    /**
     * Computes the current global charge percentage.
     * Sum of all battery charges.
     */
    int currentChargePercent() const;

    friend class Action;

    bool m_hasDualGpu;
    int m_chargeStartThreshold = 0;
    int m_chargeStopThreshold = 100;

    BackendInterface *m_backend = nullptr;

    QDBusServiceWatcher *m_notificationsWatcher = nullptr;
    bool m_notificationsReady = false;

    KSharedConfigPtr m_profilesConfig;

    QString m_currentProfile;

    QHash<QString, int> m_batteriesPercent;
    QHash<QString, int> m_peripheralBatteriesPercent;
    QHash<QString, bool> m_batteriesCharged;

    QPointer<KNotification> m_lowBatteryNotification;
    QTimer *const m_criticalBatteryTimer;
    QPointer<KNotification> m_criticalBatteryNotification;

    KActivities::Consumer *const m_activityConsumer;

    // Idle time management
    QHash< Action*, QList< int > > m_registeredActionTimeouts;
    QSet<Action *> m_pendingResumeFromIdleActions;
    bool m_pendingWakeupEvent;

    // Scheduled wakeups and alarms
    QList<WakeupInfo> m_scheduledWakeups;
    int m_lastWakeupCookie = 0;
    int m_timerFd = -1;
    QSocketNotifier *m_timerFdSocketNotifier = nullptr;

    // Activity inhibition management
    QHash< QString, int > m_sessionActivityInhibit;
    QHash< QString, int > m_screenActivityInhibit;

private Q_SLOTS:
    void onBackendReady();
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
    void onLidClosedChanged(bool closed);
    void onAboutToSuspend();
    // handlers for handling wakeup dbus call
    void resetAndScheduleNextWakeup();
    void timerfdEventHandler();
};

}

#endif // POWERDEVILCORE_H
