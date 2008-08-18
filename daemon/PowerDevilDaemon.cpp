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
#include <KAboutData>
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

#include <config-powerdevil.h>

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
    KAboutData aboutData("powerdevil", 0, ki18n("PowerDevil"),
                         POWERDEVIL_VERSION, ki18n("A Power Management tool for KDE4"), KAboutData::License_GPL,
                         ki18n("(c) 2008 PowerDevil Development Team"), ki18n("drf@kdemod.ath.cx"), "http://www.kde.org");

    aboutData.addAuthor(ki18n("Dario Freddi"), ki18n("Developer"), "drf@kdemod.ath.cx", "http://drfav.wordpress.com");

    m_applicationData = KComponentData(aboutData);

    m_profilesConfig = new KConfig( "powerdevilprofilesrc", KConfig::SimpleConfig );

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

    if (!connect(m_battery, SIGNAL(chargePercentChanged(int, const QString&)), this, SLOT(batteryChargePercentChanged(int, const QString&)))) {
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
    m_profilesConfig->reparseConfiguration();

    // Let's force status update
    acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState(), true);
}

void PowerDevilDaemon::acAdapterStateChanged(int state, bool forced)
{
    if (state == Solid::Control::PowerManager::Plugged && !forced) {

            emitNotification("pluggedin", i18n("The power adaptor has been plugged in"));
    }

    if (state == Solid::Control::PowerManager::Unplugged && !forced) {
        emitNotification("unplugged", i18n("The power adaptor has been unplugged"));
    }

    KConfigGroup *settings = getCurrentProfile(state);

    if(!settings)
        return;

    Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
    Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
            settings->readEntry("cpuPolicy").toInt());

    delete settings;

    emit stateChanged();
}

void PowerDevilDaemon::batteryChargePercentChanged(int percent, const QString &udi)
{
    Q_UNUSED(udi)

    emit errorTriggered(QString("Battery at " + percent));

    if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
        return;

    if (percent <= PowerDevilSettings::batteryCriticalLevel()) {
        emit errorTriggered("Critical Level reached");
        switch (PowerDevilSettings::batLowAction()) {
        case Shutdown:
            emitWarningNotification("criticalbattery", i18n("Your battery has reached "
                                    "critical level, the PC will now be halted..."));
            shutdown();
            break;
        case S2Disk:
            emitWarningNotification("criticalbattery", i18n("Your battery has reached "
                                    "critical level, the PC will now be suspended to disk..."));
            suspendToDisk();
            break;
        case S2Ram:
            emitWarningNotification("criticalbattery", i18n("Your battery has reached "
                                    "critical level, the PC will now be suspended to RAM..."));
            suspendToRam();
            break;
        case Standby:
            emitWarningNotification("criticalbattery", i18n("Your battery has reached "
                                    "critical level, the PC is now going Standby..."));
            standby();
            break;
        default:
            emitWarningNotification("criticalbattery", i18n("Your battery has reached "
                                    "critical level, save your work as soon as possible!"));
            break;
        }
    } else if (percent == PowerDevilSettings::batteryWarningLevel()) {
        emit errorTriggered("Warning Level reached");
        emitWarningNotification("warningbattery", i18n("Your battery has reached warning level"));
    } else if (percent == PowerDevilSettings::batteryLowLevel()) {
        emit errorTriggered("Low Level reached");
        emitWarningNotification("lowbattery", i18n("Your battery has reached low level"));
    }
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if (but == Solid::Control::PowerManager::LidClose) {

        KConfigGroup *settings = getCurrentProfile();

        if(!settings)
            return;

        switch (settings->readEntry("lidAction").toInt()) {
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

        delete settings;

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
        emitWarningNotification("joberror", QString(i18n("There was an error while suspending:")
                                + QChar('\n') + job->errorString()));
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

    KConfigGroup *settings = getCurrentProfile();

    if(!settings)
        return;

    if (idle >= settings->readEntry("idleTime").toInt() * 60) {
        switch (settings->readEntry("idleAction").toInt()) {
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
    } else if (settings->readEntry("turnOffIdle", false) &&
            (idle >= (settings->readEntry("turnOffIdleTime").toInt()))) {
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
        Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
        m_pollTimer->setInterval((PowerDevilSettings::dimOnIdleTime() * 60000 * 1 / 2) - (idle * 1000));
    }

    delete settings;
}

void PowerDevilDaemon::lockScreen()
{
    m_screenSaverIface->Lock();
}

void PowerDevilDaemon::emitWarningNotification(const QString &evid, const QString &message)
{
    if (!PowerDevilSettings::enableWarningNotifications())
        return;

    KNotification::event(evid, message,
                         KIcon("dialog-warning").pixmap(20, 20), 0, KNotification::CloseOnTimeout, m_applicationData);
}

void PowerDevilDaemon::emitNotification(const QString &evid, const QString &message)
{
    if (!PowerDevilSettings::enableNotifications())
        return;

    KNotification::event(evid, message,
                         KIcon("dialog-ok-apply").pixmap(20, 20), 0, KNotification::CloseOnTimeout, m_applicationData);
}

KConfigGroup *PowerDevilDaemon::getCurrentProfile(int state)
{
    if(state == -1)
        state = Solid::Control::PowerManager::acAdapterState();

    KConfigGroup *group;

    if (state == Solid::Control::PowerManager::Plugged)
    {
        group = new KConfigGroup(m_profilesConfig, PowerDevilSettings::aCProfile());
    }
    else if (m_battery->chargePercent() <= PowerDevilSettings::batteryWarningLevel())
    {
        group = new KConfigGroup(m_profilesConfig, PowerDevilSettings::warningProfile());
    }
    else if (m_battery->chargePercent() <= PowerDevilSettings::batteryLowLevel())
    {
        group = new KConfigGroup(m_profilesConfig, PowerDevilSettings::lowProfile());
    }
    else
    {
        group = new KConfigGroup(m_profilesConfig, PowerDevilSettings::batteryProfile());
    }

    emit errorTriggered(QString("Current Profile name is %1").arg(group->name()));

    if (!group->isValid() || !group->entryMap().size())
    {
        emit errorTriggered("Invalid group!!");
        return NULL;
    }

    return group;
}

#include "PowerDevilDaemon.moc"
