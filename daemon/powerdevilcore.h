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

#include <KComponentData>
#include <QWeakPointer>
#include <QtCore/QStringList>
#include "powerdevilbackendinterface.h"

class KNotification;
namespace Solid {
class Battery;
}

namespace PowerDevil
{

class BackendInterface;
class Action;

class KDE_EXPORT Core : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)
    Q_CLASSINFO("D-Bus Interface", "org.kde.PowerDevil")

public:
    explicit Core(QObject* parent, const KComponentData &componentData);
    virtual ~Core();

    void reloadProfile(int state = 1);

    void emitNotification(const QString &evid, const QString &message = QString(),
                          const QString &iconname = "dialog-ok-apply");

    BackendInterface *backend();
    
    // More...

public Q_SLOTS:
    // Set of common action - useful for the DBus interface
    void refreshStatus();

    void loadProfile(const QString &name);
    QString currentProfile() const;

    int brightness() const;
    void setBrightness(int percent);

    int batteryRemainingTime() const;

    void increaseBrightness();
    void decreaseBrightness();

    void suspendToRam();
    void suspendToDisk();
    void suspendHybrid();

Q_SIGNALS:
    void profileChanged(const QString &newProfile);
    void brightnessChanged(int percent);
    void batteryRemainingTimeChanged(int time);

private:
    enum ActionType {
        None = 0,
        Standby = 1,
        S2Ram = 2,
        S2Disk = 4,
        Shutdown = 8
    };

    void registerActionTimeout(Action *action, int timeout);
    void unregisterActionTimeouts(Action *action);

    friend class Action;

    BackendInterface *m_backend;
    QStringList m_loadedBatteriesUdi;

    QWeakPointer< KNotification > notification;
    KComponentData m_applicationData;
    KSharedConfigPtr m_profilesConfig;

    QString m_currentProfile;
    QStringList m_availableProfiles;

    QHash< QString, int > m_batteriesPercent;

    // Idle time management
    QHash< int, int > m_registeredIdleTimeouts;
    QHash< Action*, QList< int > > m_registeredActionTimeouts;
    QList< Action* > m_pendingResumeFromIdleActions;

private Q_SLOTS:
    void onBackendReady();
    void onBackendError(const QString &error);
    void onButtonPressed(PowerDevil::BackendInterface::ButtonType);
    void onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState);
    void onBatteryChargePercentChanged(int,const QString&);
    void onBrightnessChanged(float);
    void onBatteryRemainingTimeChanged(int);
    void onKIdleTimeoutReached(int,int);
    void onResumingFromIdle();
    void onDeviceAdded(const QString &udi);
    void onDeviceRemoved(const QString &udi);
};

}

#endif // POWERDEVILCORE_H
