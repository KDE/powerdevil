/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *   Copyright (C) 2007 by Luka Renko <lure@kubuntu.org>                   *
 *   Copyright (C) 2008 by Thomas Gillespie <tomjamesgillespie@gmail.com>  *
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

#include "PowerDevilDaemon.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KPluginLoader>
//#include <KPassivePopup>
#include <KNotification>
#include <KIcon>
#include <klocalizedstring.h>
#include <kjob.h>
#include <kworkspace/kdisplaymanager.h>

#include "PowerDevilSettings.h"
#include "powerdeviladaptor.h"

#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>

K_PLUGIN_FACTORY(PowerDevilFactory,
                 registerPlugin<PowerDevilDaemon>();)
K_EXPORT_PLUGIN(PowerDevilFactory("powerdevildaemon"))

PowerDevilDaemon::PowerDevilDaemon(QObject *parent, const QList<QVariant>&)
        : KDEDModule(parent),
        m_notifier(Solid::Control::PowerManager::notifier()),
        m_battery(0),
        m_displayManager(new KDisplayManager()),
        m_pollTimer(new QTimer())
{
    /* First of all, let's check if a battery is present. If not, this
    module has to be shut down. */

    //Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();

    bool found = false;

    //get a list of all devices that are Batteries
    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        found = true;
        Solid::Device d = device;
        m_battery = qobject_cast<Solid::Battery*>(d.asDeviceInterface(Solid::DeviceInterface::Battery));
    }

    if (!found || !m_battery) {
        //FIXME: Shut the daemon down. Is that the correct way?
        deleteLater();
    }

    m_screenSaverIface = new OrgFreedesktopScreenSaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
            QDBusConnection::sessionBus(), this);

    connect(m_notifier, SIGNAL(acAdapterStateChanged(int)), this, SLOT(acAdapterStateChanged(int)));
    connect(m_notifier, SIGNAL(buttonPressed(int)), this, SLOT(buttonPressed(int)));

    if (!connect(m_battery, SIGNAL(chargePercentChanged(int, const QString&)), this, SLOT(batteryChargePercentChanged(int, const QString&))))
    {
        emit errorTriggered("Could not connect to battery interface!");
    }

    //setup idle timer, can remove it if we can get some kind of idle time signal from dbus?
    m_pollTimer->setInterval(700);
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(poll()));
    m_pollTimer->start();

    //DBus
    new PowerDevilAdaptor(this);

    refreshStatus();
}

PowerDevilDaemon::~PowerDevilDaemon()
{
}

void PowerDevilDaemon::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    PowerDevilSettings::self()->readConfig();

    // Let's force status update
    acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState());
}

void PowerDevilDaemon::acAdapterStateChanged(int state)
{
    if (state == Solid::Control::PowerManager::Plugged) {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::aCBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
                PowerDevilSettings::aCCpuPolicy());

        emitNotification(i18n("The AC Adapter has been plugged in"));
    }

    else if (state == Solid::Control::PowerManager::Unplugged) {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::batBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
                PowerDevilSettings::batCpuPolicy());

        emitNotification(i18n("The AC Adapter has been unplugged"));
    }

    emit stateChanged();
}

void PowerDevilDaemon::batteryChargePercentChanged(int percent, const QString &udi)
{
    Q_UNUSED(udi)

    emit errorTriggered(QString("Battery at " + percent));

    if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
        return;

    if (percent <= PowerDevilSettings::batteryCriticalLevel())
    {
        emit errorTriggered("Critical Level reached");
        switch (PowerDevilSettings::batLowAction()) {
            case Shutdown:
                emitWarningNotification(i18n("Battery is at critical level, the PC will now be halted..."));
                shutdown();
                break;
            case S2Disk:
                emitWarningNotification(i18n("Battery is at critical level, the PC will now be suspended to disk..."));
                suspendToDisk();
                break;
            case S2Ram:
                emitWarningNotification(i18n("Battery is at critical level, the PC will now be suspended to RAM..."));
                suspendToRam();
                break;
            case Standby:
                emitWarningNotification(i18n("Battery is at critical level, the PC is now going Standby..."));
                standby();
                break;
            default:
                emitWarningNotification(i18n("Battery is at critical level, save your work as soon as possible!"));
                break;
        }
    }
    else if (percent == PowerDevilSettings::batteryWarningLevel())
    {
        emit errorTriggered("Warning Level reached");
        emitWarningNotification(i18n("Battery is at warning level"));
    }
    else if (percent == PowerDevilSettings::batteryLowLevel())
    {
        emit errorTriggered("Low Level reached");
        emitWarningNotification(i18n("Battery is at low level"));
    }
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if (but == Solid::Control::PowerManager::LidClose) {
        emit lidClosed(PowerDevilSettings::aCLidAction(), "Detected lid closing");
        if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged) {
            switch (PowerDevilSettings::aCLidAction()) {
            case Shutdown:
                shutdown();
                emit lidClosed((int)Shutdown, "Requested ShutDown");
                break;
            case S2Disk:
                emit lidClosed((int)S2Disk, "Requested S2Disk");
                suspendToDisk();
                break;
            case S2Ram:
                emit lidClosed((int)S2Ram, "Requested S2Ram");
                suspendToRam();
                break;
            case Standby:
                emit lidClosed((int)Standby, "Requested Standby");
                standby();
                break;
            case Lock:
                emit lidClosed((int)Lock, "Requested Lock");
                lockScreen();
                break;
            default:
                break;
            }
        } else {
            switch (PowerDevilSettings::batLidAction()) {
            case Shutdown:
                shutdown();
                break;
            case S2Disk:
                suspendToDisk();
                break;
            case S2Ram:
                suspendToRam();
                break;
            case Standby:
                standby();
                break;
            case Lock:
                lockScreen();
                break;
            default:
                break;
            }
        }
    }
}

void PowerDevilDaemon::decreaseBrightness()
{
    int currentBrightness = Solid::Control::PowerManager::brightness() - 10;

    if (currentBrightness < 0) {
        currentBrightness = 0;
    }

    Solid::Control::PowerManager::setBrightness(currentBrightness);
}

void PowerDevilDaemon::increaseBrightness()
{
    int currentBrightness = Solid::Control::PowerManager::brightness() + 10;

    if (currentBrightness > 100) {
        currentBrightness = 100;
    }

    Solid::Control::PowerManager::setBrightness(currentBrightness);
}


void PowerDevilDaemon::shutdown()
{
    m_displayManager->shutdown(KWorkSpace::ShutdownTypeHalt, KWorkSpace::ShutdownModeTryNow);
}

void PowerDevilDaemon::suspendToDisk()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendToRam()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::standby()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendJobResult(KJob * job)
{
    if (job->error()) {
        emitWarningNotification(job->errorString());
    }
    m_screenSaverIface->SimulateUserActivity(); //prevent infinite suspension loops
}

///HACK yucky
#include <QX11Info>
#include <X11/extensions/scrnsaver.h>

void PowerDevilDaemon::poll()
{
    /* ///The way that it should be done (i think)
    ------------------------------------------------------------
    int idle = m_screenSaverIface->GetSessionIdleTime();
    ------------------------------------------------------------
    */
    ///The way that works
    //----------------------------------------------------------
    XScreenSaverInfo* mitInfo = 0;
    mitInfo = XScreenSaverAllocInfo();
    XScreenSaverQueryInfo(QX11Info::display(), DefaultRootWindow(QX11Info::display()), mitInfo);
    int idle = mitInfo->idle / 1000;
    //----------------------------------------------------------

    if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged) {
        if (idle >= PowerDevilSettings::aCIdle() * 60) {
            switch (PowerDevilSettings::aCIdleAction()) {
            case Shutdown:
                shutdown();
                break;
            case S2Disk:
                suspendToDisk();
                break;
            case S2Ram:
                suspendToRam();
                break;
            case Standby:
                standby();
                break;
            default:
                break;
            }
        } else if (PowerDevilSettings::aCOffDisplayWhenIdle() &&
                   (idle >= (PowerDevilSettings::aCDisplayIdle() * 60))) {
            //FIXME Turn off monitor. How do i do this?
            //For the time being i know that the command "xset dpms force off"
            //turns off the monitor so i'll just use it
            QProcess::execute("xset dpms force off");
        } else if (PowerDevilSettings::dimOnIdle()
                   && (idle >= (PowerDevilSettings::dimOnIdleTime() * 60 * 3 / 4)))
            //threefourths time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(0);
        } else if (PowerDevilSettings::dimOnIdle() &&
                   (idle >= (PowerDevilSettings::dimOnIdleTime() * 60 * 1 / 2)))
            //half time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness() / 2);
            m_pollTimer->setInterval(2000);
        } else {
            Solid::Control::PowerManager::setBrightness(PowerDevilSettings::aCBrightness());
            m_pollTimer->setInterval((PowerDevilSettings::dimOnIdleTime() * 60000 * 1 / 2) - (idle * 1000));
        }
    } else {
        if (idle >= PowerDevilSettings::batIdle() * 60) {
            switch (PowerDevilSettings::batIdleAction()) {
            case Shutdown:
                shutdown();
                break;
            case S2Disk:
                suspendToDisk();
                break;
            case S2Ram:
                suspendToRam();
                break;
            case Standby:
                standby();
                break;
            default:
                break;
            }
        } else if (PowerDevilSettings::batOffDisplayWhenIdle() &&
                   (idle >= PowerDevilSettings::batDisplayIdle() * 60)) {
            //FIXME Turn off monitor. How do i do this?
            //For the time being i know that the command "xset dpms force off"
            //turns off the monitor so i'll use it
            QProcess::execute("xset dpms force off");
        } else if (PowerDevilSettings::dimOnIdle() &&
                   (idle >= (PowerDevilSettings::dimOnIdleTime() * 60 * 3 / 4)))
            //threefourths time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(0);
        } else if (PowerDevilSettings::dimOnIdle() &&
                   (idle >= (PowerDevilSettings::dimOnIdleTime() * 60 * 1 / 2)))
            //half time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness() / 2);
            m_pollTimer->setInterval(2000);
        } else {
            Solid::Control::PowerManager::setBrightness(PowerDevilSettings::batBrightness());
            m_pollTimer->setInterval((PowerDevilSettings::dimOnIdleTime() * 60000 * 1 / 2) - (idle * 1000));
        }
    }
}

void PowerDevilDaemon::lockScreen()
{
    m_screenSaverIface->Lock();
}

void PowerDevilDaemon::emitWarningNotification(const QString &message)
{
    if (!PowerDevilSettings::enableWarningNotifications())
        return;

    KNotification::event(KNotification::Warning, message,
            KIcon("dialog-warning").pixmap(20, 20));
}

void PowerDevilDaemon::emitNotification(const QString &message)
{
    if (!PowerDevilSettings::enableNotifications())
            return;

    KNotification::event(KNotification::Notification, message,
            KIcon("dialog-ok-apply").pixmap(20, 20));
}

#include "PowerDevilDaemon.moc"
