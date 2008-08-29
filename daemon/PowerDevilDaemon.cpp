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
 ***************************************************************************
 *                                                                         *
 *   Part of the code in this file was taken from KDE4Powersave and/or     *
 *   Lithium, where noticed.                                               *
 *                                                                         *
 **************************************************************************/

#include "PowerDevilDaemon.h"

#include <kdemacros.h>
#include <KAboutData>
#include <KPluginFactory>
#include <KNotification>
#include <KIcon>
#include <klocalizedstring.h>
#include <kjob.h>
#include <kworkspace/kdisplaymanager.h>

#include <QWidget>
#include <QTimer>

#include "PowerDevilSettings.h"
#include "powerdeviladaptor.h"

#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <config-powerdevil.h>

K_PLUGIN_FACTORY(PowerDevilFactory,
                 registerPlugin<PowerDevilDaemon>();)
K_EXPORT_PLUGIN(PowerDevilFactory("powerdevildaemon"))

PowerDevilDaemon::PowerDevilDaemon(QObject *parent, const QList<QVariant>&)
        : KDEDModule(parent),
        m_notifier(Solid::Control::PowerManager::notifier()),
        m_battery(0),
        m_displayManager(new KDisplayManager()),
        m_pollTimer(new QTimer(this))
{
    KGlobal::locale()->insertCatalog("powerdevil");

    KAboutData aboutData("powerdevil", "powerdevil", ki18n("PowerDevil"),
                         POWERDEVIL_VERSION, ki18n("A Power Management tool for KDE4"), KAboutData::License_GPL,
                         ki18n("(c) 2008 PowerDevil Development Team"), KLocalizedString(), "http://www.kde.org");

    aboutData.addAuthor(ki18n("Dario Freddi"), ki18n("Developer"), "drf@kdemod.ath.cx", "http://drfav.wordpress.com");

    m_applicationData = KComponentData(aboutData);

    m_profilesConfig = new KConfig("powerdevilprofilesrc", KConfig::SimpleConfig);
    m_availableProfiles = m_profilesConfig->groupList();

    /* You'll see some switches on m_battery. This is fundamental since PowerDevil might run
     * also on system without batteries. Most of modern desktop systems support CPU scaling,
     * so somebody might find PowerDevil handy, and we don't want it to crash on them. To put it
     * short, we simply bypass all adaptor and battery events if no batteries are found.
     */

    // Here we get our battery interface, it will be useful later.
    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Solid::Device d = device;
        m_battery = qobject_cast<Solid::Battery*>(d.asDeviceInterface(Solid::DeviceInterface::Battery));
    }

    m_screenSaverIface = new OrgFreedesktopScreenSaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
            QDBusConnection::sessionBus(), this);

    connect(m_notifier, SIGNAL(buttonPressed(int)), this, SLOT(buttonPressed(int)));

    /* Those slots are relevant only if we're on a system that has a battery. If not, we simply don't care
     * about them.
     */
    if (m_battery) {
        connect(m_notifier, SIGNAL(acAdapterStateChanged(int)), this, SLOT(acAdapterStateChanged(int)));

        if (!connect(m_battery, SIGNAL(chargePercentChanged(int, const QString&)), this,
                     SLOT(batteryChargePercentChanged(int, const QString&)))) {
            emit errorTriggered(i18n("Could not connect to battery interface!"));
        }
    }

    //setup idle timer, with some smart polling
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(poll()));

    // This code was taken from Lithium/KDE4Powersave
    m_grabber = new QWidget(0, Qt::X11BypassWindowManagerHint);
    m_grabber->move(-1000, -1000);
    m_grabber->setMouseTracking(true);
    m_grabber->installEventFilter(this);
    m_grabber->setObjectName("PowerDevilGrabberWidget");

    //DBus
    new PowerDevilAdaptor(this);

    refreshStatus();
}

PowerDevilDaemon::~PowerDevilDaemon()
{
    delete m_profilesConfig;
    delete m_displayManager;
}

bool PowerDevilDaemon::eventFilter(QObject* object, QEvent* event)
{
    emit pollEvent(i18n("Filtering event %1 from %2", event->type(), object->objectName()));

    if ((object == m_grabber)
            && (event->type() == QEvent::MouseMove || event->type() == QEvent::KeyPress)) {
        detectedActivity();
    } else if (object != m_grabber) {
        // If it's not the grabber, fallback to default event filter
        return KDEDModule::eventFilter(object, event);
    }

    // Otherwise, simply ignore it
    return false;

}

void PowerDevilDaemon::detectedActivity()
{
    // This code was taken from Lithium/KDE4Powersave

    m_grabber->releaseMouse();
    m_grabber->releaseKeyboard();
    m_grabber->hide();

    emit pollEvent(i18n("Detected Activity"));

    refreshStatus();
}

void PowerDevilDaemon::waitForActivity()
{
    // This code was taken from Lithium/KDE4Powersave

    m_grabber->show();
    m_grabber->grabMouse();
    m_grabber->grabKeyboard();
}

void PowerDevilDaemon::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    PowerDevilSettings::self()->readConfig();
    m_profilesConfig->reparseConfiguration();

    reloadProfile();

    /* Let's force status update, if we have a battery. Otherwise, let's just
     * re-apply the current profile.
     */
    if (m_battery) {
        acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState(), true);
    } else {
        applyProfile();
    }
}

void PowerDevilDaemon::acAdapterStateChanged(int state, bool forced)
{
    if (state == Solid::Control::PowerManager::Plugged && !forced) {
        m_isPlugged = true;
        emitNotification("pluggedin", i18n("The power adaptor has been plugged in"));
    }

    if (state == Solid::Control::PowerManager::Unplugged && !forced) {
        m_isPlugged = false;
        emitNotification("unplugged", i18n("The power adaptor has been unplugged"));
    }

    if (!forced)
        reloadProfile(state);

    applyProfile();
}

void PowerDevilDaemon::applyProfile()
{
    KConfigGroup *settings = getCurrentProfile();

    if (!settings)
        return;

    Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
    Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
            settings->readEntry("cpuPolicy").toInt());

    QVariant var = settings->readEntry("disabledCPUs", QVariant());
    QList<QVariant> list = var.toList();

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*>(d.asDeviceInterface(Solid::DeviceInterface::Processor));

        bool enable = true;

        foreach(const QVariant &ent, list) {
            if (processor->number() == ent.toInt()) {
                enable = false;
            }
        }

        Solid::Control::PowerManager::setCpuEnabled(processor->number(), enable);
    }

    Solid::Control::PowerManager::setScheme(settings->readEntry("scheme"));

    delete settings;

    emit stateChanged(m_batteryPercent, m_isPlugged);
}

void PowerDevilDaemon::batteryChargePercentChanged(int percent, const QString &udi)
{
    Q_UNUSED(udi)

    m_batteryPercent = percent;

    if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
        return;

    if (percent <= PowerDevilSettings::batteryCriticalLevel()) {
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
        emitWarningNotification("warningbattery", i18n("Your battery has reached warning level"));
        refreshStatus();
    } else if (percent == PowerDevilSettings::batteryLowLevel()) {
        emitWarningNotification("lowbattery", i18n("Your battery has reached low level"));
        refreshStatus();
    }

    emit stateChanged(m_batteryPercent, m_isPlugged);
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if (but == Solid::Control::PowerManager::LidClose) {

        KConfigGroup *settings = getCurrentProfile();

        if (!settings)
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
    emitNotification("doingjob", i18n("The computer is being halted"));
    m_displayManager->shutdown(KWorkSpace::ShutdownTypeHalt, KWorkSpace::ShutdownModeTryNow);
}

void PowerDevilDaemon::suspendToDisk()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    emitNotification("doingjob", i18n("The computer will now be suspended to disk"));

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendToRam()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    emitNotification("doingjob", i18n("The computer will now be suspended to RAM"));

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::standby()
{
    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    emitNotification("doingjob", i18n("The computer will now be put into standby"));

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
    /* This poll function behaves smartly. In fact, polling happens only
     * on-demand. This function is called on the following situations: loading,
     * profile change, resuming, and upon events.
     * As you can see below, the timer is in fact started with the minimum
     * time from next event.
     * The idea was taken from KDE4Powersave & Lithium, so kudos to them.
     * We make an intensive use of qMin/qMax here to determine the minimum time.
     */

    /* Hack! Since KRunner still doesn't behave properly, the
     * correct way to go doesn't work (yet), and it's this one:
    ------------------------------------------------------------
    int idle = m_screenSaverIface->GetSessionIdleTime();
    ------------------------------------------------------------
    */
    /// In the meanwhile, this X11 hackish way gets its job done.
    //----------------------------------------------------------
    XScreenSaverInfo* mitInfo = 0;
    mitInfo = XScreenSaverAllocInfo();
    XScreenSaverQueryInfo(QX11Info::display(), DefaultRootWindow(QX11Info::display()), mitInfo);
    int idle = mitInfo->idle / 1000;
    //----------------------------------------------------------

    emit pollEvent(i18n("Polling started, idle time is %1 seconds", idle));

    KConfigGroup *settings = getCurrentProfile();

    if (!settings) {
        return;
    }

    if (!PowerDevilSettings::dimOnIdle() && !settings->readEntry("turnOffIdle", false) &&
            settings->readEntry("idleAction").toInt() == None) {
        emit pollEvent(i18n("Stopping the timer"));
        m_pollTimer->stop();
        return;
    }

    int dimOnIdleTime = PowerDevilSettings::dimOnIdleTime() * 60;
    int minDimTime = dimOnIdleTime * 1 / 2;
    int minDimEvent = dimOnIdleTime;

    emit pollEvent(i18n("dimOnIdleTime is %1 seconds", PowerDevilSettings::dimOnIdleTime() * 60));

    if (idle < (dimOnIdleTime * 3 / 4)) {
        minDimEvent = dimOnIdleTime * 3 / 4;
    }
    if (idle < (dimOnIdleTime * 1 / 2)) {
        minDimEvent = dimOnIdleTime * 1 / 2;
    }

    emit pollEvent(i18n("minDimEvent is %1 seconds", minDimEvent));

    int minTime = settings->readEntry("idleTime").toInt() * 60;
    emit pollEvent(i18n("idleTime is %1 seconds", settings->readEntry("idleTime").toInt() * 60));

    if (settings->readEntry("turnOffIdle", QVariant()).toBool()) {
        minTime = qMin(minTime, settings->readEntry("turnOffIdleTime").toInt() * 60);
        emit pollEvent(i18n("turnoffidleTime is %1 seconds", settings->readEntry("turnOffIdleTime").toInt() * 60));
    }
    if (PowerDevilSettings::dimOnIdle()) {
        minTime = qMin(minTime, minDimTime);
    }

    emit pollEvent(i18n("Minimum time is %1 seconds", minTime));

    if (idle < minTime) {
        int remaining = minTime - idle;
        m_pollTimer->start(remaining * 1000);
        emit pollEvent(i18n("Nothing to do, next event in %1 seconds", remaining));
        return;
    }

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

    } else if (settings->readEntry("turnOffIdle", QVariant()).toBool() &&
               (idle >= (settings->readEntry("turnOffIdleTime").toInt() * 60))) {
        /* FIXME: This command works, though we can switch to dpms... need some
         * feedback here.
         */
        QProcess::execute("xset dpms force off");
    } else if (PowerDevilSettings::dimOnIdle()
               && (idle >= dimOnIdleTime)) {
        Solid::Control::PowerManager::setBrightness(0);
    } else if (PowerDevilSettings::dimOnIdle()
               && (idle >= (dimOnIdleTime * 3 / 4))) {
        float newBrightness = Solid::Control::PowerManager::brightness() / 4;
        Solid::Control::PowerManager::setBrightness(newBrightness);
    } else if (PowerDevilSettings::dimOnIdle() &&
               (idle >= (dimOnIdleTime * 1 / 2))) {
        float newBrightness = Solid::Control::PowerManager::brightness() / 2;
        Solid::Control::PowerManager::setBrightness(newBrightness);
    } else {
        Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
    }

    int nextTimeout = -1;

    if ((settings->readEntry("idleTime").toInt() * 60) > idle) {
        if (nextTimeout >= 0) {
            nextTimeout = qMin(nextTimeout, (settings->readEntry("idleTime").toInt() * 60) - idle);
        } else {
            nextTimeout = (settings->readEntry("idleTime").toInt() * 60) - idle;
        }
    }
    if ((settings->readEntry("turnOffIdleTime").toInt() * 60) > idle &&
            settings->readEntry("turnOffIdle", QVariant()).toBool()) {
        if (nextTimeout >= 0) {
            nextTimeout = qMin(nextTimeout, (settings->readEntry("turnOffIdleTime").toInt() * 60) - idle);
        } else {
            nextTimeout = (settings->readEntry("turnOffIdleTime").toInt() * 60) - idle;
        }
    }
    if (minDimEvent > idle && PowerDevilSettings::dimOnIdle()) {
        if (nextTimeout >= 0) {
            nextTimeout = qMin(nextTimeout, minDimEvent - idle);
        } else {
            nextTimeout = minDimEvent - idle;
        }
    }

    if (nextTimeout >= 0) {
        m_pollTimer->start(nextTimeout * 1000);
        emit pollEvent(i18n("Next timeout in %1 seconds", nextTimeout));
    } else {
        m_pollTimer->stop();
        emit pollEvent(i18n("Stopping timer"));
    }

    delete settings;

    waitForActivity();
}

void PowerDevilDaemon::lockScreen()
{
    emitNotification("doingjob", i18n("The screen is being locked"));
    m_screenSaverIface->Lock();
}

void PowerDevilDaemon::emitWarningNotification(const QString &evid, const QString &message)
{
    if (!PowerDevilSettings::enableWarningNotifications())
        return;

    KNotification::event(evid, message, KIcon("dialog-warning").pixmap(20, 20),
                         0, KNotification::CloseOnTimeout, m_applicationData);
}

void PowerDevilDaemon::emitNotification(const QString &evid, const QString &message)
{
    if (!PowerDevilSettings::enableNotifications())
        return;

    KNotification::event(evid, message, KIcon("dialog-ok-apply").pixmap(20, 20),
                         0, KNotification::CloseOnTimeout, m_applicationData);
}

KConfigGroup *PowerDevilDaemon::getCurrentProfile()
{
    KConfigGroup *group = new KConfigGroup(m_profilesConfig, m_currentProfile);

    emit errorTriggered(i18n("Current Profile name is %1", group->name()));

    if (!group->isValid() || !group->entryMap().size()) {
        emit errorTriggered(i18n("Invalid group!!"));
        reloadProfile();
        delete group;
        return NULL;
    }

    return group;
}

void PowerDevilDaemon::reloadProfile(int state)
{
    if (!m_battery) {
        m_currentProfile = PowerDevilSettings::aCProfile();
    } else {
        if (state == -1)
            state = Solid::Control::PowerManager::acAdapterState();

        if (state == Solid::Control::PowerManager::Plugged) {
            m_currentProfile = PowerDevilSettings::aCProfile();
        } else if (m_battery->chargePercent() <= PowerDevilSettings::batteryWarningLevel()) {
            m_currentProfile = PowerDevilSettings::warningProfile();
        } else if (m_battery->chargePercent() <= PowerDevilSettings::batteryLowLevel()) {
            m_currentProfile = PowerDevilSettings::lowProfile();
        } else {
            m_currentProfile = PowerDevilSettings::batteryProfile();
        }
    }

    emit profileChanged(m_currentProfile, m_availableProfiles);

    poll();
}

void PowerDevilDaemon::setProfile(const QString & profile)
{
    m_currentProfile = profile;
    refreshStatus();
    emitNotification("profileset", i18n("Profile changed to \"%1\"", profile));
    emit profileChanged(m_currentProfile, m_availableProfiles);
}

void PowerDevilDaemon::reloadAndStream()
{
    if (!m_battery) {
        m_currentProfile = PowerDevilSettings::aCProfile();
        m_isPlugged = true;

        m_batteryPercent = 100;
    } else {
        if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged) {
            m_currentProfile = PowerDevilSettings::aCProfile();
            m_isPlugged = true;
        } else if (m_battery->chargePercent() <= PowerDevilSettings::batteryWarningLevel()) {
            m_currentProfile = PowerDevilSettings::warningProfile();
            m_isPlugged = false;
        } else if (m_battery->chargePercent() <= PowerDevilSettings::batteryLowLevel()) {
            m_currentProfile = PowerDevilSettings::lowProfile();
            m_isPlugged = false;
        } else {
            m_currentProfile = PowerDevilSettings::batteryProfile();
            m_isPlugged = false;
        }

        m_batteryPercent = Solid::Control::PowerManager::batteryChargePercent();
    }

    m_availableProfiles = m_profilesConfig->groupList();

    emit profileChanged(m_currentProfile, m_availableProfiles);
    emit stateChanged(m_batteryPercent, m_isPlugged);

    refreshStatus();
}

QStringList PowerDevilDaemon::getSupportedGovernors()
{
    QStringList retlist;

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies & Solid::Control::PowerManager::Performance) {
        retlist.append(i18n("Performance"));
    }

    if (policies & Solid::Control::PowerManager::OnDemand) {
        retlist.append(i18n("Dynamic (ondemand)"));
    }

    if (policies & Solid::Control::PowerManager::Conservative) {
        retlist.append(i18n("Dynamic (conservative)"));
    }

    if (policies & Solid::Control::PowerManager::Powersave) {
        retlist.append(i18n("Powersave"));
    }

    if (policies & Solid::Control::PowerManager::Userspace) {
        retlist.append(i18n("Userspace"));
    }

    return retlist;
}

QStringList PowerDevilDaemon::getSupportedSchemes()
{
    return Solid::Control::PowerManager::supportedSchemes();
}

void PowerDevilDaemon::setPowersavingScheme(const QString &scheme)
{
    Solid::Control::PowerManager::setScheme(scheme);
}

void PowerDevilDaemon::setGovernor(const QString &governor)
{
    if (governor == i18n("Performance")) {
        Solid::Control::PowerManager::setCpuFreqPolicy(Solid::Control::PowerManager::Performance);
    } else if (governor == i18n("Dynamic (ondemand)")) {
        Solid::Control::PowerManager::setCpuFreqPolicy(Solid::Control::PowerManager::OnDemand);
    } else if (governor == i18n("Dynamic (conservative)")) {
        Solid::Control::PowerManager::setCpuFreqPolicy(Solid::Control::PowerManager::Conservative);
    } else if (governor == i18n("Powersave")) {
        Solid::Control::PowerManager::setCpuFreqPolicy(Solid::Control::PowerManager::Powersave);
    } else if (governor == i18n("Userspace")) {
        Solid::Control::PowerManager::setCpuFreqPolicy(Solid::Control::PowerManager::Userspace);
    }
}

void PowerDevilDaemon::setBrightness(int value)
{
    Solid::Control::PowerManager::setBrightness(value);
}

#include "PowerDevilDaemon.moc"
