/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#ifndef POWERDEVILDAEMON_H
#define POWERDEVILDAEMON_H

#include <kdedmodule.h>
#include <solid/control/powermanager.h>
#include <solid/battery.h>
#include <KComponentData>
#include <QStringList>
#include <QPointer>

#include "AbstractSystemPoller.h"

class QWidget;
class QTimer;
class PollSystemLoader;
class SuspensionLockHandler;
class KNotification;
class OrgFreedesktopScreenSaverInterface;
class OrgKdeKSMServerInterfaceInterface;
class OrgKdeScreensaverInterface;

class KDE_EXPORT PowerDevilDaemon : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.PowerDevil")

public:
    PowerDevilDaemon(QObject *parent, const QList<QVariant>&);
    virtual ~PowerDevilDaemon();

public Q_SLOTS:
    void refreshStatus();
    void emitWarningNotification(const QString &evid, const QString &message = QString(),
                                 const char *slot = 0, const QString &iconname = "dialog-warning");
    void emitNotification(const QString &evid, const QString &message = QString(),
                          const char *slot = 0, const QString &iconname = "dialog-ok-apply");
    void setProfile(const QString & profile);
    void reloadAndStream();
    void streamData();

    void setGovernor(int governor);
    void suspend(int method);
    void setPowersavingScheme(const QString &scheme);
    void setBrightness(int value);
    void turnOffScreen();

    void setUpPollingSystem();

    void unloadDaemon() {
        deleteLater();
    };

    QVariantMap getSupportedGovernors();
    QStringList getSupportedSchemes();
    QVariantMap getSupportedSuspendMethods();
    QVariantMap getSupportedPollingSystems();

    SuspensionLockHandler *lockHandler() {
        return m_lockHandler;
    };

private Q_SLOTS:
    void acAdapterStateChanged(int state, bool forced = false);
    void batteryChargePercentChanged(int percent, const QString &udi);

    void decreaseBrightness();
    void increaseBrightness();

    void shutdown(bool automated = false);
    void shutdownDialog();
    void suspendJobResult(KJob * job);
    void suspendToDisk(bool automated = false);
    void suspendToRam(bool automated = false);
    void standby(bool automated = false);

    void shutdownNotification(bool automated = false);
    void suspendToDiskNotification(bool automated = false);
    void suspendToRamNotification(bool automated = false);
    void standbyNotification(bool automated = false);

    void buttonPressed(int but);

    void poll(int idle);
    void resumeFromIdle();

    void reloadProfile(int state = -1);
    const QString &profile() {
        return m_currentProfile;
    }

    void setBatteryPercent(int newpercent);
    void setACPlugged(bool newplugged);
    void setCurrentProfile(const QString &profile);
    void setAvailableProfiles(const QStringList &aProfiles);

    bool toggleCompositing(bool enabled);

    void cleanUpTimer();

    void setUpDPMS();

    void emitCriticalNotification(const QString &evid, const QString &message = QString(),
                                  const char *slot = 0, const QString &iconname = "dialog-error");

Q_SIGNALS:
    void lidClosed(int code, const QString &action);
    void errorTriggered(const QString &error);

    void stateChanged(int, bool);
    void profileChanged(const QString &, const QStringList &);

private:
    void lockScreen();

    KConfigGroup *getCurrentProfile(bool forcereload = false);
    void applyProfile();

    void setUpNextTimeout(int idle, int minDimEvent);

    void profileFirstLoad();

    void restoreDefaultProfiles();

    bool loadPollingSystem(AbstractSystemPoller::PollingType type);

    bool recacheBatteryPointer(bool force = false);

public:
    enum IdleAction {
        None = 0,
        Standby = 1,
        S2Ram = 2,
        S2Disk = 4,
        Shutdown = 8,
        Lock = 16,
        ShutdownDialog = 32
    };

private:
    Solid::Control::PowerManager::Notifier * m_notifier;
    QPointer<Solid::Battery> m_battery;

    OrgFreedesktopScreenSaverInterface * m_screenSaverIface;
    OrgKdeKSMServerInterfaceInterface * m_ksmServerIface;
    OrgKdeScreensaverInterface * m_kscreenSaverIface;

    QWidget * m_grabber;

    KComponentData m_applicationData;
    KSharedConfig::Ptr m_profilesConfig;
    KConfigGroup * m_currentConfig;
    PollSystemLoader * m_pollLoader;
    SuspensionLockHandler * m_lockHandler;

    QString m_currentProfile;
    QStringList m_availableProfiles;

    KNotification *m_notification;
    QTimer *m_notificationTimer;

    int m_batteryPercent;
    bool m_isPlugged;
    bool m_compositingChanged;
};

#endif /*POWERDEVILDAEMON_H*/
