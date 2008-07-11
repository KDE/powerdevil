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
#include <KPassivePopup>
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
 m_displayManager(new KDisplayManager()),
 m_pollTimer(new QTimer())
{
    /* First of all, let's check if a battery is present. If not, this
    module has to be shut down. */

    //Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
	
    bool found = false;
    //get a list of all devices that are Batteries
    foreach (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString()))
      found = true;
    
    if ( !found )
    {
      //TODO: Shut the daemon down.
    }

    m_screenSaverIface = new OrgFreedesktopScreenSaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver", 
                                                                 QDBusConnection::sessionBus(), this);

    connect(m_notifier, SIGNAL(acAdapterStateChanged(int)), this, SLOT(acAdapterStateChanged(int)));
    connect(m_notifier, SIGNAL(batteryStateChanged(int)), this, SLOT(batteryStateChanged(int)));
    connect(m_notifier, SIGNAL(buttonPressed(int)), this, SLOT(buttonPressed(int)));
    
    //setup idle timer, can remove it if we can get some kind of idle time signal from dbus?
    m_pollTimer->setInterval(700);
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(poll()));
    m_pollTimer->start();
    
    //DBus
    new PowerDevilAdaptor( this );
    
    refreshStatus();
}

PowerDevilDaemon::~PowerDevilDaemon()
{
}

void PowerDevilDaemon::refreshStatus()
{
    //Setup initial state
    acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState());
}

void PowerDevilDaemon::acAdapterStateChanged(int state)
{
    if ( state == Solid::Control::PowerManager::Plugged )
    {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::aCBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
	PowerDevilSettings::aCCpuPolicy());
    }
    
    else if ( state == Solid::Control::PowerManager::Unplugged )
    {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::batBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
	PowerDevilSettings::batCpuPolicy());
    }
    
    emit stateChanged();
}

void PowerDevilDaemon::batteryStateChanged(int state)
{
    switch(state)
    {
      //FIXME: Is this the right way?
        case Solid::Control::PowerManager::Critical:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at critical level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
        case Solid::Control::PowerManager::Warning:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at warning level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
        case Solid::Control::PowerManager::Low:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at low level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
    }
    
    if(state == Solid::Control::PowerManager::Critical && 
      Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Unplugged)
    {
        switch(PowerDevilSettings::batLowAction())
        {
            case Shutdown:
                shutdown();
                break;
            case S2Disk:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                break;
            case S2Ram:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                break;
            case Standby:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
                break;
            default:
                break;
        }
    }
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if(but == Solid::Control::PowerManager::LidClose)
    {
        if(Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
        {
            switch(PowerDevilSettings::aCLidAction())
            {
                case Shutdown:
                    shutdown();
                    break;
                case S2Disk:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                    break;
                case S2Ram:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                    break;
                case Standby:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
                    break;
                case Lock:
                    lockScreen();
                    break;
                default:
                    break;
            }
        }
        else
        {
            switch(PowerDevilSettings::batLidAction())
            {
                case Shutdown:
                    shutdown();
                    break;
                case S2Disk:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                    break;
                case S2Ram:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                    break;
                case Standby:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
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
    
    if(currentBrightness < 0)
    {
        currentBrightness = 0;
    }
    
    Solid::Control::PowerManager::setBrightness(currentBrightness);
}

void PowerDevilDaemon::increaseBrightness()
{
    int currentBrightness = Solid::Control::PowerManager::brightness() + 10;
    
    if(currentBrightness > 100)
    {
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
    if(PowerDevilSettings::configLockScreen())
    {
        lockScreen();
    }
    
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendToRam()
{
    if(PowerDevilSettings::configLockScreen())
    {
        lockScreen();
    }
    
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::standby()
{
    if(PowerDevilSettings::configLockScreen())
    {
        lockScreen();
    }
    
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendJobResult(KJob * job)
{
    if(job->error())
    {
        KPassivePopup::message(KPassivePopup::Boxed, i18n("PowerDevil"), job->errorString(),
                           KIcon("dialog-warning").pixmap(20,20), dynamic_cast<QWidget *>(this), 15000);
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
    mitInfo = XScreenSaverAllocInfo ();
    XScreenSaverQueryInfo (QX11Info::display(), DefaultRootWindow (QX11Info::display()), mitInfo);
    int idle = mitInfo->idle/1000;
    //----------------------------------------------------------

    if(Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
    {        
        if(idle >= PowerDevilSettings::aCIdle() * 60)
        {
            switch(PowerDevilSettings::aCIdleAction())
            {
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
        }
        else if(PowerDevilSettings::aCOffDisplayWhenIdle() &&
	  (idle >= (PowerDevilSettings::aCDisplayIdle() * 60)))
        {
            //FIXME Turn off monitor. How do i do this?
            //For the time being i know that the command "xset dpms force off"
            //turns off the monitor so i'll just use it
            QProcess::execute("xset dpms force off");
        }
        else if(PowerDevilSettings::dimOnIdle() 
	  && (idle >= (PowerDevilSettings::aCDisplayIdle() * 60 * 3/4))) 
	  //threefourths time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(0);
        }
        else if(PowerDevilSettings::dimOnIdle() && 
	  (idle >= (PowerDevilSettings::aCDisplayIdle() * 60 * 1/2))) 
	  //half time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness() / 2);
            m_pollTimer->setInterval(2000);
        }
        else
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness());
            m_pollTimer->setInterval((PowerDevilSettings::aCDisplayIdle() * 60000 * 1/2) - (idle * 1000));
        }
    } 
    else
    {
        if(idle >= PowerDevilSettings::batIdle() * 60)
        {
            switch(PowerDevilSettings::batIdleAction())
            {
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
        }
        else if(PowerDevilSettings::batOffDisplayWhenIdle() &&
	  (idle >= PowerDevilSettings::batDisplayIdle() * 60))
        {
            //FIXME Turn off monitor. How do i do this?
            //For the time being i know that the command "xset dpms force off"
            //turns off the monitor so i'll use it
            QProcess::execute("xset dpms force off");
        }
        else if(PowerDevilSettings::dimOnIdle() && 
	  (idle >= (PowerDevilSettings::batDisplayIdle() * 60 * 3/4))) 
	  //threefourths time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(0);
        }
        else if(PowerDevilSettings::dimOnIdle() && 
	  (idle >= (PowerDevilSettings::batDisplayIdle() * 60 * 1/2))) 
	  //half time - written this way for readability
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness() / 2);
            m_pollTimer->setInterval(2000);
        }
        else
        {
            Solid::Control::PowerManager::setBrightness(Solid::Control::PowerManager::brightness());
            m_pollTimer->setInterval((PowerDevilSettings::batDisplayIdle() * 60000 * 1/2) - (idle * 1000));
        }
    }
}

void PowerDevilDaemon::lockScreen()
{
    m_screenSaverIface->Lock();
}

#include "PowerDevilDaemon.moc"
