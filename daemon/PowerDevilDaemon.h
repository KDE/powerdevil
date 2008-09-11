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
#include "screensaver_interface.h"
#include <KComponentData>

class KDisplayManager;
class QWidget;
class QTimer;

class KDE_EXPORT PowerDevilDaemon : public KDEDModule
{
        Q_OBJECT

    public:
        PowerDevilDaemon( QObject *parent, const QList<QVariant>& );
        virtual ~PowerDevilDaemon();

    public Q_SLOTS:
        void refreshStatus();
        void emitWarningNotification( const QString &evid, const QString &message = QString(),
                                      const QString &iconname = "dialog-warning" );
        void emitNotification( const QString &evid, const QString &message = QString(),
                               const QString &iconname = "dialog-ok-apply" );
        void setProfile( const QString & profile );
        void reloadAndStream();
        void streamData();

        void setGovernor( const QString &governor );
        void setPowersavingScheme( const QString &scheme );
        void setBrightness( int value );
        void turnOffScreen();

        void unloadDaemon() {
            deleteLater();
        };

        QStringList getSupportedGovernors();
        QStringList getSupportedSchemes();

    private Q_SLOTS:
        void acAdapterStateChanged( int state, bool forced = false );
        void batteryChargePercentChanged( int percent, const QString &udi );

        void decreaseBrightness();
        void increaseBrightness();

        void shutdown();
        void suspendJobResult( KJob * job );
        void suspendToDisk();
        void suspendToRam();
        void standby();

        void buttonPressed( int but );

        void poll();
        void detectedActivity();
        void waitForActivity();

        void reloadProfile( int state = -1 );
        const QString &profile() {
            return m_currentProfile;
        }

    Q_SIGNALS:
        void lidClosed( int code, const QString &action );
        void errorTriggered( const QString &error );

        void stateChanged( int, bool );
        void profileChanged( const QString &, const QStringList & );

        void pollEvent( const QString &event );

    protected:
        bool eventFilter( QObject * object, QEvent * event );

    private:
        void lockScreen();
        KConfigGroup *getCurrentProfile();
        void applyProfile();
        void releaseInputLock();

        void emitCriticalNotification( const QString &evid, const QString &message = QString(),
                                       const QString &iconname = "dialog-error" );

    private:
        enum IdleAction {
            Shutdown = 1,
            S2Disk = 2,
            S2Ram = 3,
            Standby = 4,
            Lock = 5,
            None = 0
        };

        Solid::Control::PowerManager::Notifier * m_notifier;
        Solid::Battery * m_battery;

        KDisplayManager * m_displayManager;
        OrgFreedesktopScreenSaverInterface * m_screenSaverIface;

        QTimer * m_pollTimer;
        QWidget * m_grabber;

        KComponentData m_applicationData;
        KConfig * m_profilesConfig;

        QString m_currentProfile;
        QStringList m_availableProfiles;

        int m_batteryPercent;
        bool m_isPlugged;
};

#endif /*POWERDEVILDAEMON_H*/
