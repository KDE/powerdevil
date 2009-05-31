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
#include <KMessageBox>
#include <kpluginfactory.h>
#include <klocalizedstring.h>
#include <kjob.h>
#include <kworkspace/kworkspace.h>
#include <KApplication>

#include <QPointer>
#include <QWidget>
#include <QTimer>

#include "PowerDevilSettings.h"
#include "powerdeviladaptor.h"
#include "PowerManagementConnector.h"
#include "PollSystemLoader.h"
#include "SuspensionLockHandler.h"

#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include "screensaver_interface.h"
//#include "kscreensaver_interface.h"
#include "ksmserver_interface.h"

#include <config-powerdevil.h>
#include <config-workspace.h>

#ifdef HAVE_DPMS
#include <X11/Xmd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
extern "C" {
#include <X11/extensions/dpms.h>
    int __kde_do_not_unload = 1;

#ifndef HAVE_DPMSCAPABLE_PROTO
    Bool DPMSCapable(Display *);
#endif

#ifndef HAVE_DPMSINFO_PROTO
    Status DPMSInfo(Display *, CARD16 *, BOOL *);
#endif
}

static XErrorHandler defaultHandler;

#endif

#define POLLER_CALL(Object, Method) \
    if (Object != 0) { \
        AbstractSystemPoller *t = qobject_cast<AbstractSystemPoller *>(Object); \
        if (t!=0) { \
            t->Method; \
        } \
    } else { \
        kWarning() << "WARNING: No poller system loaded, PowerDevil can not detect idle time"; \
    }

K_PLUGIN_FACTORY(PowerDevilFactory,
                 registerPlugin<PowerDevilDaemon>();)
K_EXPORT_PLUGIN(PowerDevilFactory("powerdevildaemon"))

class PowerDevilDaemon::Private
{
public:
    explicit Private()
            : notifier(Solid::Control::PowerManager::notifier())
            , battery(0)
            , currentConfig(0)
            , status(PowerDevilDaemon::NoAction)
            , ckSessionInterface(0) {}

    Solid::Control::PowerManager::Notifier *notifier;
    QPointer<Solid::Battery> battery;

    OrgFreedesktopScreenSaverInterface *screenSaverIface;
    OrgKdeKSMServerInterfaceInterface *ksmServerIface;
//  Now we send a signal to trigger the configuration of Kscreensaver (Bug #177123) and we don't need the interface anymore
//     OrgKdeScreensaverInterface * kscreenSaverIface;

    KComponentData applicationData;
    KSharedConfig::Ptr profilesConfig;
    KConfigGroup *currentConfig;
    PollSystemLoader *pollLoader;
    SuspensionLockHandler *lockHandler;

    QString currentProfile;
    QStringList availableProfiles;

    QPointer<KNotification> notification;
    QTimer *notificationTimer;

    PowerDevilDaemon::IdleStatus status;

    int batteryPercent;
    bool isPlugged;

    // ConsoleKit stuff
    QDBusInterface *ckSessionInterface;
    bool ckAvailable;
};

PowerDevilDaemon::PowerDevilDaemon(QObject *parent, const QList<QVariant>&)
        : KDEDModule(parent),
        d(new Private())
{
    KGlobal::locale()->insertCatalog("powerdevil");

    KAboutData aboutData("powerdevil", "powerdevil", ki18n("PowerDevil"),
                         POWERDEVIL_VERSION, ki18n("A Power Management tool for KDE4"),
                         KAboutData::License_GPL, ki18n("(c) 2008 Dario Freddi"),
                         KLocalizedString(), "http://www.kde.org");

    aboutData.addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer"), "drf54321@gmail.com",
                        "http://drfav.wordpress.com");

    d->applicationData = KComponentData(aboutData);

    d->pollLoader = new PollSystemLoader(this);
    d->lockHandler = new SuspensionLockHandler(this);
    d->notificationTimer = new QTimer(this);

    /* First things first: PowerDevil might be used when another powermanager is already
     * on. So we need to check the system bus and see if another known powermanager has
     * already been registered. Let's see.
     */

    QDBusConnection conn = QDBusConnection::systemBus();

    if (conn.interface()->isServiceRegistered("org.freedesktop.PowerManagement") ||
            conn.interface()->isServiceRegistered("com.novell.powersave") ||
            conn.interface()->isServiceRegistered("org.freedesktop.Policy.Power")) {
        kError() << "PowerDevil not initialized, another power manager has been detected";
        return;
    }

    // First things first: let's set up PowerDevil to be aware of active session
    setUpConsoleKit();

    d->profilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);
    setAvailableProfiles(d->profilesConfig->groupList());

    recacheBatteryPointer(true);

    // Set up all needed DBus interfaces
    d->screenSaverIface = new OrgFreedesktopScreenSaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
            QDBusConnection::sessionBus(), this);
    d->ksmServerIface = new OrgKdeKSMServerInterfaceInterface("org.kde.ksmserver", "/KSMServer",
            QDBusConnection::sessionBus(), this);

    /*  Not needed anymore; I am not sure if we will need that in a future, so I leave it here
     *  just in case.
     *
     *   d->kscreenSaverIface = new OrgKdeScreensaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
     *         QDBusConnection::sessionBus(), this);
    */
    connect(d->notifier, SIGNAL(buttonPressed(int)), this, SLOT(buttonPressed(int)));
    connect(d->notifier, SIGNAL(batteryRemainingTimeChanged(int)), this, SLOT(batteryRemainingTimeChanged(int)));
    connect(d->lockHandler, SIGNAL(streamCriticalNotification(const QString&, const QString&,
                                   const char*, const QString&)),
            SLOT(emitCriticalNotification(const QString&, const QString&,
                                          const char*, const QString&)));

    /* Time for setting up polling! We can have different methods, so
     * let's check what we got.
     */

    if (PowerDevilSettings::pollingSystem() == -1) {
        // Ok, new configuration... so let's see what we've got!!

        QHash<AbstractSystemPoller::PollingType, QString> pList = d->pollLoader->getAvailableSystems();

        if (pList.contains(AbstractSystemPoller::XSyncBased)) {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::XSyncBased);
        } else if (pList.contains(AbstractSystemPoller::WidgetBased)) {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::WidgetBased);
        } else {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::TimerBased);
        }

        PowerDevilSettings::self()->writeConfig();
    }

    setUpPollingSystem();

    //DBus
    new PowerDevilAdaptor(this);
    new PowerManagementConnector(this);

    conn.interface()->registerService("org.freedesktop.Policy.Power");
    QDBusConnection::sessionBus().registerService("org.kde.powerdevil");
    // All systems up Houston, let's go!
    refreshStatus();
}

PowerDevilDaemon::~PowerDevilDaemon()
{
    delete d->ckSessionInterface;
    delete d;
}

void PowerDevilDaemon::batteryRemainingTimeChanged(int time)
{
    kDebug() << KGlobal::locale()->formatDuration(time);
}

SuspensionLockHandler *PowerDevilDaemon::lockHandler()
{
    return d->lockHandler;
}

QString PowerDevilDaemon::profile() const
{
    return d->currentProfile;
}

bool PowerDevilDaemon::recacheBatteryPointer(bool force)
{
    /* You'll see some switches on d->battery. This is fundamental since PowerDevil might run
     * also on system without batteries. Most of modern desktop systems support CPU scaling,
     * so somebody might find PowerDevil handy, and we don't want it to crash on them. To put it
     * short, we simply bypass all adaptor and battery events if no batteries are found.
     */

    if (d->battery) {
        if (d->battery->isValid() && !force) {
            return true;
        }
    }

    d->battery = 0;

    // Here we get our battery interface, it will be useful later.
    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Solid::Device dev = device;
        Solid::Battery *b = qobject_cast<Solid::Battery*> (dev.asDeviceInterface(Solid::DeviceInterface::Battery));

        if (b->type() != Solid::Battery::PrimaryBattery) {
            continue;
        }

        if (b->isValid()) {
            d->battery = b;
        }
    }

    /* Those slots are relevant only if we're on a system that has a battery. If not, we simply don't care
     * about them.
     */
    if (d->battery) {
        connect(d->notifier, SIGNAL(acAdapterStateChanged(int)), this, SLOT(acAdapterStateChanged(int)));

        if (!connect(d->battery, SIGNAL(chargePercentChanged(int, const QString &)), this,
                     SLOT(batteryChargePercentChanged(int, const QString &)))) {

            emitCriticalNotification("powerdevilerror", i18n("Could not connect to battery interface.\n"
                                     "Please check your system configuration"));
            return false;
        }
    } else {
        return false;
    }

    return true;
}

void PowerDevilDaemon::setUpPollingSystem()
{
    if (!loadPollingSystem((AbstractSystemPoller::PollingType) PowerDevilSettings::pollingSystem())) {
        /* Let's try to load each profile one at a time, and then
         * set the configuration to the profile that worked out.
         */

        if (loadPollingSystem(AbstractSystemPoller::XSyncBased)) {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::XSyncBased);
            PowerDevilSettings::self()->writeConfig();
            return;
        }

        if (loadPollingSystem(AbstractSystemPoller::WidgetBased)) {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::WidgetBased);
            PowerDevilSettings::self()->writeConfig();
            return;
        }

        if (loadPollingSystem(AbstractSystemPoller::TimerBased)) {
            PowerDevilSettings::setPollingSystem(AbstractSystemPoller::TimerBased);
            PowerDevilSettings::self()->writeConfig();
            return;
        }

        /* If we're here, we have a big problem, since no polling system has been loaded.
         * What should we do? For now, let's just spit out a kError
         */

        kError() << "Could not load a polling system!";
    }
}

bool PowerDevilDaemon::loadPollingSystem(AbstractSystemPoller::PollingType type)
{
    QHash<AbstractSystemPoller::PollingType, QString> pList = d->pollLoader->getAvailableSystems();

    if (!pList.contains(type)) {
        return false;
    } else {
        if (!d->pollLoader->loadSystem(type)) {
            return false;
        }
    }

    if (d->pollLoader->poller()) {
        connect(d->pollLoader->poller(), SIGNAL(resumingFromIdle()), SLOT(resumeFromIdle()));
        connect(d->pollLoader->poller(), SIGNAL(pollRequest(int)), SLOT(poll(int)));
    } else {
        return false;
    }

    return true;
}

QVariantMap PowerDevilDaemon::getSupportedPollingSystems()
{
    QVariantMap map;

    QHash<int, QString> pmap = d->pollLoader->getAvailableSystemsAsInt();

    QHash<int, QString>::const_iterator i;
    for (i = pmap.constBegin(); i != pmap.constEnd(); ++i) {
        map[i.value()] = i.key();
    }

    return map;
}

void PowerDevilDaemon::resumeFromIdle()
{
    KConfigGroup * settings = getCurrentProfile();

    POLLER_CALL(d->pollLoader->poller(), stopCatchingIdleEvents());
    POLLER_CALL(d->pollLoader->poller(), forcePollRequest());

    if (!checkIfCurrentSessionActive()) {
        return;
    }

    Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
}

void PowerDevilDaemon::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    PowerDevilSettings::self()->readConfig();
    d->profilesConfig->reparseConfiguration();

    reloadProfile();

    getCurrentProfile(true);

    /* Let's force status update, if we have a battery. Otherwise, let's just
     * re-apply the current profile.
     */
    if (d->battery) {
        acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState(), true);
    } else {
        applyProfile();
    }
}

void PowerDevilDaemon::acAdapterStateChanged(int state, bool forced)
{
    if (state == Solid::Control::PowerManager::Plugged && !forced) {
        setACPlugged(true);

        // If the AC Adaptor has been plugged in, let's clear some pending suspend actions
        if (!d->lockHandler->canStartNotification()) {
            cleanUpTimer();
            d->lockHandler->releaseNotificationLock();
            emitNotification("pluggedin", i18n("The power adaptor has been plugged in. Any pending "
                                               "suspend actions have been canceled."));
        } else {
            emitNotification("pluggedin", i18n("The power adaptor has been plugged in."));
        }
    }

    if (state == Solid::Control::PowerManager::Unplugged && !forced) {
        setACPlugged(false);
        emitNotification("unplugged", i18n("The power adaptor has been unplugged."));
    }

    if (!forced) {
        reloadProfile(state);
    }

    applyProfile();
}

#ifdef HAVE_DPMS
extern "C" {
    int dropError(Display *, XErrorEvent *);
    typedef int (*XErrFunc)(Display *, XErrorEvent *);
}

int dropError(Display *, XErrorEvent *)
{
    return 0;
}
#endif

void PowerDevilDaemon::applyProfile()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    KConfigGroup * settings = getCurrentProfile();

    if (!settings) {
        return;
    }

    Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
    Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
            settings->readEntry("cpuPolicy").toInt());

    QVariant var = settings->readEntry("disabledCPUs", QVariant());
    QList<QVariant> list = var.toList();

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        Solid::Device d = device;
        Solid::Processor * processor = qobject_cast<Solid::Processor * > (d.asDeviceInterface(Solid::DeviceInterface::Processor));

        bool enable = true;

        foreach(const QVariant &ent, list) {
            if (processor->number() == ent.toInt()) {
                enable = false;
            }
        }

        Solid::Control::PowerManager::setCpuEnabled(processor->number(), enable);
    }

    Solid::Control::PowerManager::setScheme(settings->readEntry("scheme"));

    POLLER_CALL(d->pollLoader->poller(), forcePollRequest());
}

void PowerDevilDaemon::setUpDPMS()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    KConfigGroup * settings = getCurrentProfile();

    if (!settings) {
        return;
    }

#ifdef HAVE_DPMS

    defaultHandler = XSetErrorHandler(dropError);

    Display *dpy = QX11Info::display();

    int dummy;
    bool has_DPMS = true;

    if (!DPMSQueryExtension(dpy, &dummy, &dummy) || !DPMSCapable(dpy)) {
        has_DPMS = false;
        XSetErrorHandler(defaultHandler);
    }

    if (has_DPMS) {

        if (settings->readEntry("DPMSEnabled", false)) {
            DPMSEnable(dpy);
        } else {
            DPMSDisable(dpy);
        }

        XFlush(dpy);
        XSetErrorHandler(defaultHandler);

        //

        int standby = 60 * settings->readEntry("DPMSStandby").toInt();
        int suspend = 60 * settings->readEntry("DPMSSuspend").toInt();
        int poff = 60 * settings->readEntry("DPMSPowerOff").toInt();

        if (!settings->readEntry("DPMSStandbyEnabled", false)) {
            standby = 0;
        }
        if (!settings->readEntry("DPMSSuspendEnabled", false)) {
            suspend = 0;
        }
        if (!settings->readEntry("DPMSPowerOffEnabled", false)) {
            poff = 0;
        }

        DPMSSetTimeouts(dpy, standby, suspend, poff);

        XFlush(dpy);
        XSetErrorHandler(defaultHandler);

    }

    // The screen saver depends on the DPMS settings
    // Emit a signal so that Kscreensaver knows it has to re-configure itself
    emit DPMSconfigUpdated();
#endif
}

void PowerDevilDaemon::batteryChargePercentChanged(int percent, const QString &udi)
{
    Q_UNUSED(udi)
    Q_UNUSED(percent)

    int charge = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Solid::Device d = device;
        Solid::Battery *battery = qobject_cast<Solid::Battery*> (d.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (battery->chargePercent() > 0 && battery->type() == Solid::Battery::PrimaryBattery) {
            charge += battery->chargePercent();
        }
    }

    setBatteryPercent(charge);

    if (Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged) {
        return;
    }

    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (charge <= PowerDevilSettings::batteryCriticalLevel()) {
        switch (PowerDevilSettings::batLowAction()) {
        case Shutdown:
            if (PowerDevilSettings::waitBeforeSuspending()) {
                emitWarningNotification("criticalbattery",
                                        i18np("Your battery level is critical, the computer will be halted in 1 second.",
                                              "Your battery level is critical, the computer will be halted in %1 seconds.",
                                              PowerDevilSettings::waitBeforeSuspendingTime()),
                                        SLOT(shutdown()));
            } else {
                shutdown();
            }
            break;
        case S2Disk:
            if (PowerDevilSettings::waitBeforeSuspending()) {
                emitWarningNotification("criticalbattery",
                                        i18np("Your battery level is critical, the computer will be suspended to disk in 1 second.",
                                              "Your battery level is critical, the computer will be suspended to disk in %1 seconds.",
                                              PowerDevilSettings::waitBeforeSuspendingTime()),
                                        SLOT(suspendToDisk()));
            } else {
                suspendToDisk();
            }
            break;
        case S2Ram:
            if (PowerDevilSettings::waitBeforeSuspending()) {
                emitWarningNotification("criticalbattery",
                                        i18np("Your battery level is critical, the computer will be suspended to RAM in 1 second.",
                                              "Your battery level is critical, the computer will be suspended to RAM in %1 seconds.",
                                              PowerDevilSettings::waitBeforeSuspendingTime()),
                                        SLOT(suspendToRam()));
            } else {
                suspendToRam();
            }
            break;
        case Standby:
            if (PowerDevilSettings::waitBeforeSuspending()) {
                emitWarningNotification("criticalbattery",
                                        i18np("Your battery level is critical, the computer will be put into standby in 1 second.",
                                              "Your battery level is critical, the computer will be put into standby in %1 seconds.",
                                              PowerDevilSettings::waitBeforeSuspendingTime()),
                                        SLOT(standby()));
            } else {
                standby();
            }
            break;
        default:
            emitWarningNotification("criticalbattery", i18n("Your battery level is critical: save your work as soon as possible."));
            break;
        }
    } else if (charge == PowerDevilSettings::batteryWarningLevel()) {
        emitWarningNotification("warningbattery", i18n("Your battery has reached the warning level."));
        refreshStatus();
    } else if (charge == PowerDevilSettings::batteryLowLevel()) {
        emitWarningNotification("lowbattery", i18n("Your battery has reached a low level."));
        refreshStatus();
    }
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if (!checkIfCurrentSessionActive() || d->screenSaverIface->GetActive()) {
        return;
    }

    KConfigGroup * settings = getCurrentProfile();

    if (!settings)
        return;

    kDebug() << "A button was pressed, code" << but;

    if (but == Solid::Control::PowerManager::LidClose) {

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
        case TurnOffScreen:
            turnOffScreen();
            break;
        default:
            break;
        }
    } else if (but == Solid::Control::PowerManager::PowerButton) {

        switch (settings->readEntry("powerButtonAction").toInt()) {
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
        case ShutdownDialog:
            shutdownDialog();
            break;
        case TurnOffScreen:
            turnOffScreen();
            break;
        default:
            break;
        }
    } else if (but == Solid::Control::PowerManager::SleepButton) {

        switch (settings->readEntry("sleepButtonAction").toInt()) {
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
        case ShutdownDialog:
            shutdownDialog();
            break;
        case TurnOffScreen:
            turnOffScreen();
            break;
        default:
            break;
        }
    }
}

void PowerDevilDaemon::decreaseBrightness()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    int currentBrightness = qMax(0, (int)(Solid::Control::PowerManager::brightness() - 10));
    Solid::Control::PowerManager::setBrightness(currentBrightness);
}

void PowerDevilDaemon::increaseBrightness()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    int currentBrightness = qMin(100, (int)(Solid::Control::PowerManager::brightness() + 10));
    Solid::Control::PowerManager::setBrightness(currentBrightness);
}

void PowerDevilDaemon::shutdownNotification(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setNotificationLock(automated)) {
        return;
    }

    if (PowerDevilSettings::waitBeforeSuspending()) {
        emitNotification("doingjob", i18np("The computer will be halted in 1 second.",
                                           "The computer will be halted in %1 seconds.",
                                           PowerDevilSettings::waitBeforeSuspendingTime()),
                         SLOT(shutdown()));
    } else {
        shutdown();
    }
}

void PowerDevilDaemon::suspendToDiskNotification(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setNotificationLock(automated)) {
        return;
    }

    if (PowerDevilSettings::waitBeforeSuspending()) {
        emitNotification("doingjob", i18np("The computer will be suspended to disk in 1 second.",
                                           "The computer will be suspended to disk in %1 seconds.",
                                           PowerDevilSettings::waitBeforeSuspendingTime()),
                         SLOT(suspendToDisk()));
    } else {
        suspendToDisk();
    }
}

void PowerDevilDaemon::suspendToRamNotification(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setNotificationLock(automated)) {
        return;
    }

    if (PowerDevilSettings::waitBeforeSuspending()) {
        emitNotification("doingjob", i18np("The computer will be suspended to RAM in 1 second.",
                                           "The computer will be suspended to RAM in %1 seconds.",
                                           PowerDevilSettings::waitBeforeSuspendingTime()),
                         SLOT(suspendToRam()));
    } else {
        suspendToRam();
    }
}

void PowerDevilDaemon::standbyNotification(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setNotificationLock(automated)) {
        return;
    }

    if (PowerDevilSettings::waitBeforeSuspending()) {
        emitNotification("doingjob", i18np("The computer will be put into standby in 1 second.",
                                           "The computer will be put into standby in %1 seconds.",
                                           PowerDevilSettings::waitBeforeSuspendingTime()),
                         SLOT(standby()));
    } else {
        standby();
    }
}

void PowerDevilDaemon::shutdown(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setJobLock(automated)) {
        return;
    }

    d->ksmServerIface->logout((int)KWorkSpace::ShutdownConfirmNo, (int)KWorkSpace::ShutdownTypeHalt,
                              (int)KWorkSpace::ShutdownModeTryNow);

    d->lockHandler->releaseAllLocks();
}

void PowerDevilDaemon::shutdownDialog()
{
    d->ksmServerIface->logout((int)KWorkSpace::ShutdownConfirmYes, (int)KWorkSpace::ShutdownTypeDefault,
                              (int)KWorkSpace::ShutdownModeDefault);
}

void PowerDevilDaemon::suspendToDisk(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setJobLock(automated)) {
        return;
    }

    POLLER_CALL(d->pollLoader->poller(), simulateUserActivity()); //prevent infinite suspension loops

    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();

    // Temporary hack...
    QTimer::singleShot(10000, d->lockHandler, SLOT(releaseAllLocks()));
}

void PowerDevilDaemon::suspendToRam(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setJobLock(automated)) {
        return;
    }

    POLLER_CALL(d->pollLoader->poller(), simulateUserActivity()); //prevent infinite suspension loops

    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
    // Temporary hack...
    QTimer::singleShot(10000, d->lockHandler, SLOT(releaseAllLocks()));
}

void PowerDevilDaemon::standby(bool automated)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!d->lockHandler->setJobLock(automated)) {
        return;
    }

    POLLER_CALL(d->pollLoader->poller(), simulateUserActivity()); //prevent infinite suspension loops

    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();

    // Temporary hack...
    QTimer::singleShot(10000, d->lockHandler, SLOT(releaseAllLocks()));
}

void PowerDevilDaemon::suspendJobResult(KJob * job)
{
    if (job->error()) {
        emitCriticalNotification("joberror", QString(i18n("There was an error while suspending:")
                                 + QChar('\n') + job->errorString()));
    }

    POLLER_CALL(d->pollLoader->poller(), simulateUserActivity()); //prevent infinite suspension loops

    kDebug() << "Resuming from suspension";

    d->lockHandler->releaseAllLocks();

    job->deleteLater();
}

void PowerDevilDaemon::poll(int idle)
{
    /* The polling system is abstract. This function gets called when the current
     * system requests it. Usually, it is called only when needed (if you're using
     * an efficient backend such as XSync).
     * We make an intensive use of qMin/qMax here to determine the minimum time.
     */

    // kDebug() << "Polling started, idle time is" << idle << "seconds";

    if (!checkIfCurrentSessionActive()) {
        return;
    }

    KConfigGroup * settings = getCurrentProfile();

    if (!settings) {
        return;
    }

    if (!settings->readEntry("dimOnIdle", false) && !settings->readEntry("turnOffIdle", false) &&
            settings->readEntry("idleAction").toInt() == None) {
        //   kDebug() << "Stopping timer";
        POLLER_CALL(d->pollLoader->poller(), stopCatchingTimeouts());
        return;
    }

    int dimOnIdleTime = settings->readEntry("dimOnIdleTime").toInt() * 60;
    int minDimTime = dimOnIdleTime * 1 / 2;
    int minDimEvent = dimOnIdleTime;

    if (idle < (dimOnIdleTime * 3 / 4)) {
        minDimEvent = dimOnIdleTime * 3 / 4;
    }
    if (idle < (dimOnIdleTime * 1 / 2)) {
        minDimEvent = dimOnIdleTime * 1 / 2;
    }

    int minTime = settings->readEntry("idleTime").toInt() * 60;

    if (settings->readEntry("turnOffIdle", false)) {
        minTime = qMin(minTime, settings->readEntry("turnOffIdleTime").toInt() * 60);
    }
    if (settings->readEntry("dimOnIdle", false)) {
        minTime = qMin(minTime, minDimTime);
    }

    // kDebug() << "Minimum time is" << minTime << "seconds";

    if (idle < minTime) {
        d->status = NoAction;
        int remaining = minTime - idle;
        POLLER_CALL(d->pollLoader->poller(), setNextTimeout(remaining * 1000));
        //   kDebug() << "Nothing to do, next event in" << remaining << "seconds";
        return;
    }

    /* You'll see we release input lock here sometimes. Why? Well,
     * after some tests, I found out that the offscreen widget doesn't work
     * if the monitor gets turned off or the PC is suspended. But, we don't care
     * about this for a simple reason: the only parameter we need to look into
     * is the brightness, so we just release the lock, set back the brightness
     * to normal, and that's it.
     */

    if (idle >= settings->readEntry("idleTime").toInt() * 60) {
        setUpNextTimeout(idle, minDimEvent);

        if (d->status == Action) {
            return;
        }

        d->status = Action;

        switch (settings->readEntry("idleAction").toInt()) {
        case Shutdown:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            shutdownNotification(true);
            break;
        case S2Disk:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            suspendToDiskNotification(true);
            break;
        case S2Ram:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            suspendToRamNotification(true);
            break;
        case Standby:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            standbyNotification(true);
            break;
        case Lock:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            lockScreen();
            break;
        case TurnOffScreen:
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            turnOffScreen();
            break;
        default:
            break;
        }

        return;

    } else if (settings->readEntry("dimOnIdle", false)
               && (idle >= dimOnIdleTime)) {
        if (d->status != DimTotal) {
            d->status = DimTotal;
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            Solid::Control::PowerManager::setBrightness(0);
        }
    } else if (settings->readEntry("dimOnIdle", false)
               && (idle >= (dimOnIdleTime * 3 / 4))) {
        if (d->status != DimThreeQuarters) {
            d->status = DimThreeQuarters;
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            float newBrightness = Solid::Control::PowerManager::brightness() / 4;
            Solid::Control::PowerManager::setBrightness(newBrightness);
        }
    } else if (settings->readEntry("dimOnIdle", false) &&
               (idle >= (dimOnIdleTime * 1 / 2))) {
        if (d->status != DimHalf) {
            d->status = DimHalf;
            POLLER_CALL(d->pollLoader->poller(), catchIdleEvent());
            float newBrightness = Solid::Control::PowerManager::brightness() / 2;
            Solid::Control::PowerManager::setBrightness(newBrightness);
        }
    } else {
        d->status = NoAction;
        POLLER_CALL(d->pollLoader->poller(), stopCatchingIdleEvents());
        Solid::Control::PowerManager::setBrightness(settings->readEntry("brightness").toInt());
    }

    setUpNextTimeout(idle, minDimEvent);
}

void PowerDevilDaemon::setUpNextTimeout(int idle, int minDimEvent)
{
    KConfigGroup *settings = getCurrentProfile();

    int nextTimeout = -1;

    if ((settings->readEntry("idleTime").toInt() * 60) > idle) {
        if (nextTimeout >= 0) {
            nextTimeout = qMin(nextTimeout, (settings->readEntry("idleTime").toInt() * 60) - idle);
        } else {
            nextTimeout = (settings->readEntry("idleTime").toInt() * 60) - idle;
        }
    }
    if (minDimEvent > idle && settings->readEntry("dimOnIdle", false)) {
        if (nextTimeout >= 0) {
            nextTimeout = qMin(nextTimeout, minDimEvent - idle);
        } else {
            nextTimeout = minDimEvent - idle;
        }
    }

    if (nextTimeout >= 0) {
        POLLER_CALL(d->pollLoader->poller(), setNextTimeout(nextTimeout * 1000));
        kDebug() << "Next timeout in" << nextTimeout << "seconds";
    } else {
        POLLER_CALL(d->pollLoader->poller(), stopCatchingTimeouts());
        kDebug() << "Stopping timer";
    }
}

void PowerDevilDaemon::lockScreen()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    emitNotification("doingjob", i18n("The screen is being locked"));
    d->screenSaverIface->Lock();
}

void PowerDevilDaemon::emitCriticalNotification(const QString &evid, const QString &message,
        const char *slot, const QString &iconname)
{
    /* Those notifications are always displayed */
    if (!slot) {
        KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                             0, KNotification::CloseOnTimeout, d->applicationData);
    } else {
        d->notification = KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                                               0, KNotification::Persistent, d->applicationData);
        d->notification->setActions(QStringList() << i18nc("Interrupts the suspension/shutdown process", "Abort Action"));

        connect(d->notificationTimer, SIGNAL(timeout()), slot);
        connect(d->notificationTimer, SIGNAL(timeout()), SLOT(cleanUpTimer()));

        d->lockHandler->connect(d->notification, SIGNAL(activated(unsigned int)), d->lockHandler, SLOT(releaseNotificationLock()));
        connect(d->notification, SIGNAL(activated(unsigned int)), SLOT(cleanUpTimer()));

        d->notificationTimer->start(PowerDevilSettings::waitBeforeSuspendingTime() * 1000);
    }
}

void PowerDevilDaemon::emitWarningNotification(const QString &evid, const QString &message,
        const char *slot, const QString &iconname)
{
    if (!PowerDevilSettings::enableWarningNotifications()) {
        if (slot) {
            QTimer::singleShot(0, this, slot);
        }
        return;
    }

    if (!slot) {
        KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                             0, KNotification::CloseOnTimeout, d->applicationData);
    } else {
        d->notification = KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                                               0, KNotification::Persistent, d->applicationData);
        d->notification->setActions(QStringList() << i18nc("Interrupts the suspension/shutdown process", "Abort Action"));

        connect(d->notificationTimer, SIGNAL(timeout()), slot);
        connect(d->notificationTimer, SIGNAL(timeout()), SLOT(cleanUpTimer()));

        d->lockHandler->connect(d->notification, SIGNAL(activated(unsigned int)), d->lockHandler, SLOT(releaseNotificationLock()));
        connect(d->notification, SIGNAL(activated(unsigned int)), SLOT(cleanUpTimer()));

        d->notificationTimer->start(PowerDevilSettings::waitBeforeSuspendingTime() * 1000);
    }
}

void PowerDevilDaemon::emitNotification(const QString &evid, const QString &message,
                                        const char *slot, const QString &iconname)
{
    if (!PowerDevilSettings::enableNotifications()) {
        if (slot) {
            QTimer::singleShot(0, this, slot);
        }
        return;
    }

    if (!slot) {
        KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                             0, KNotification::CloseOnTimeout, d->applicationData);
    } else {
        d->notification = KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                                               0, KNotification::Persistent, d->applicationData);
        d->notification->setActions(QStringList() << i18nc("Interrupts the suspension/shutdown process", "Abort Action"));

        connect(d->notificationTimer, SIGNAL(timeout()), slot);
        connect(d->notificationTimer, SIGNAL(timeout()), SLOT(cleanUpTimer()));

        d->lockHandler->connect(d->notification, SIGNAL(activated(unsigned int)), d->lockHandler, SLOT(releaseNotificationLock()));
        connect(d->notification, SIGNAL(activated(unsigned int)), SLOT(cleanUpTimer()));

        d->notificationTimer->start(PowerDevilSettings::waitBeforeSuspendingTime() * 1000);
    }
}

void PowerDevilDaemon::cleanUpTimer()
{
    kDebug() << "Disconnecting signals";

    d->notificationTimer->disconnect();
    d->notificationTimer->stop();

    if (d->notification) {
        d->notification->disconnect();
        d->notification->deleteLater();
    }
}

KConfigGroup * PowerDevilDaemon::getCurrentProfile(bool forcereload)
{
    /* We need to access this a lot of times, so we use a cached
     * implementation here. We create the object just if we're sure
     * it is not already valid.
     *
     * IMPORTANT!!! This class already handles deletion of the config
     * object, so you don't have to delete it!!
     */

    if (d->currentConfig) {   // This HAS to be kept, since d->currentConfig could be not valid!!
        if (forcereload || d->currentConfig->name() != d->currentProfile) {
            delete d->currentConfig;
            d->currentConfig = 0;
        }
    }

    if (!d->currentConfig) {
        d->currentConfig = new KConfigGroup(d->profilesConfig, d->currentProfile);
    }

    if (!d->currentConfig->isValid() || !d->currentConfig->entryMap().size()) {
        emitCriticalNotification("powerdevilerror", i18n("The profile \"%1\" has been selected, "
                                 "but it does not exist.\nPlease check your PowerDevil configuration.",
                                 d->currentProfile));
        reloadProfile();
        delete d->currentConfig;
        d->currentConfig = 0;
    }

    return d->currentConfig;
}

void PowerDevilDaemon::reloadProfile(int state)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (!recacheBatteryPointer()) {
        setCurrentProfile(PowerDevilSettings::aCProfile());
    } else {
        if (state == -1) {
            state = Solid::Control::PowerManager::acAdapterState();
        }

        if (state == Solid::Control::PowerManager::Plugged) {
            setCurrentProfile(PowerDevilSettings::aCProfile());
        } else if (d->battery->chargePercent() <= PowerDevilSettings::batteryWarningLevel()) {
            setCurrentProfile(PowerDevilSettings::warningProfile());
        } else if (d->battery->chargePercent() <= PowerDevilSettings::batteryLowLevel()) {
            setCurrentProfile(PowerDevilSettings::lowProfile());
        } else {
            setCurrentProfile(PowerDevilSettings::batteryProfile());
        }
    }

    if (d->currentProfile.isEmpty()) {
        /* Ok, misconfiguration! Well, first things first: if we have some profiles,
         * let's just load the first available one.
         */

        if (!d->availableProfiles.isEmpty()) {
            setCurrentProfile(d->availableProfiles.at(0));
        } else {
            /* In this case, let's fill our profiles file with our
             * wonderful defaults!
             */

            restoreDefaultProfiles();

            PowerDevilSettings::setACProfile("Performance");
            PowerDevilSettings::setBatteryProfile("Powersave");
            PowerDevilSettings::setLowProfile("Aggressive Powersave");
            PowerDevilSettings::setWarningProfile("Xtreme Powersave");

            PowerDevilSettings::self()->writeConfig();

            reloadAndStream();

            return;
        }
    }

    POLLER_CALL(d->pollLoader->poller(), forcePollRequest());
}

void PowerDevilDaemon::setProfile(const QString & profile)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    setCurrentProfile(profile);

    /* Don't call refreshStatus() here, since we don't want the predefined profile
     * to be loaded!!
     */

    applyProfile();

    KConfigGroup * settings = getCurrentProfile();

    emitNotification("profileset", i18n("Profile changed to \"%1\"", profile),
                     0, settings->readEntry("iconname"));
}

void PowerDevilDaemon::reloadAndStream()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    reloadProfile();

    setAvailableProfiles(d->profilesConfig->groupList());

    streamData();

    refreshStatus();
}

void PowerDevilDaemon::streamData()
{
    emit profileChanged(d->currentProfile, d->availableProfiles);
    emit stateChanged(d->batteryPercent, d->isPlugged);
}

QVariantMap PowerDevilDaemon::getSupportedGovernors()
{
    QVariantMap retlist;

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies & Solid::Control::PowerManager::Performance) {
        retlist[i18n("Performance")] = (int) Solid::Control::PowerManager::Performance;
    }

    if (policies & Solid::Control::PowerManager::OnDemand) {
        retlist[i18n("Dynamic (ondemand)")] = (int) Solid::Control::PowerManager::OnDemand;
    }

    if (policies & Solid::Control::PowerManager::Conservative) {
        retlist[i18n("Dynamic (conservative)")] = (int) Solid::Control::PowerManager::Conservative;
    }

    if (policies & Solid::Control::PowerManager::Powersave) {
        retlist[i18n("Powersave")] = (int) Solid::Control::PowerManager::Powersave;
    }

    if (policies & Solid::Control::PowerManager::Userspace) {
        retlist[i18n("Userspace")] = (int) Solid::Control::PowerManager::Userspace;
    }

    return retlist;
}

QVariantMap PowerDevilDaemon::getSupportedSuspendMethods()
{
    QVariantMap retlist;

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods & Solid::Control::PowerManager::ToDisk) {
        retlist[i18n("Suspend to Disk")] = (int) S2Disk;
    }

    if (methods & Solid::Control::PowerManager::ToRam) {
        retlist[i18n("Suspend to RAM")] = (int) S2Ram;
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        retlist[i18n("Standby")] = (int) Standby;
    }

    return retlist;
}

QStringList PowerDevilDaemon::getSupportedSchemes()
{
    return Solid::Control::PowerManager::supportedSchemes();
}

void PowerDevilDaemon::setPowersavingScheme(const QString &scheme)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    Solid::Control::PowerManager::setScheme(scheme);
}

void PowerDevilDaemon::setGovernor(int governor)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy) governor);
}

void PowerDevilDaemon::suspend(int method)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    switch ((IdleAction) method) {
    case S2Disk:
        QTimer::singleShot(100, this, SLOT(suspendToDisk()));
        break;
    case S2Ram:
        QTimer::singleShot(100, this, SLOT(suspendToRam()));
        break;
    case Standby:
        QTimer::singleShot(100, this, SLOT(standby()));
        break;
    default:
        break;
    }
}

void PowerDevilDaemon::setBrightness(int value)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (value == -2) {
        // Then set brightness to half the current brightness.

        float b = Solid::Control::PowerManager::brightness() / 2;
        Solid::Control::PowerManager::setBrightness(b);
        return;
    }
    Solid::Control::PowerManager::setBrightness(value);
}

void PowerDevilDaemon::turnOffScreen()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (PowerDevilSettings::configLockScreen()) {
        lockScreen();
    }

#ifdef HAVE_DPMS

    CARD16 dummy;
    BOOL enabled;
    Display *dpy = QX11Info::display();

    DPMSInfo(dpy, &dummy, &enabled);

    if (enabled) {
        DPMSForceLevel(dpy, DPMSModeOff);
    } else {
        DPMSEnable(dpy);
        DPMSForceLevel(dpy, DPMSModeOff);
    }
#endif
}

void PowerDevilDaemon::profileFirstLoad()
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    KConfigGroup * settings = getCurrentProfile();

    if (!settings)
        return;

    kDebug() << "Profile initialization";

    if (!settings->readEntry("scriptpath", QString()).isEmpty()) {
        QProcess::startDetached(settings->readEntry("scriptpath"));
    }

    // Compositing!!

    if (settings->readEntry("disableCompositing", false)) {
        if (toggleCompositing(false)) {
            PowerDevilSettings::setCompositingChanged(true);
            PowerDevilSettings::self()->writeConfig();
        }
    } else if (PowerDevilSettings::compositingChanged()) {
        toggleCompositing(true);
        PowerDevilSettings::setCompositingChanged(false);
        PowerDevilSettings::self()->writeConfig();
    }

    if (PowerDevilSettings::manageDPMS()) {
        setUpDPMS();
    }
}

bool PowerDevilDaemon::toggleCompositing(bool enabled)
{
    QDBusInterface kwiniface("org.kde.kwin", "/KWin", "org.kde.KWin", QDBusConnection::sessionBus());

    QDBusReply<bool> state = kwiniface.call("compositingActive");

    if (state.value() != enabled) {
        kwiniface.call("toggleCompositing");
        return true;
    } else {
        return false;
    }
}

void PowerDevilDaemon::restoreDefaultProfiles()
{
    QString path = QString("%1/default.powerdevilprofiles").arg(DATA_DIRECTORY);

    KConfig toImport(path, KConfig::SimpleConfig);

    foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup copyFrom(&toImport, ent);
        KConfigGroup copyTo(d->profilesConfig, ent);

        copyFrom.copyTo(&copyTo);
    }

    d->profilesConfig->sync();
}

void PowerDevilDaemon::setBatteryPercent(int newpercent)
{
    d->batteryPercent = newpercent;
    emit stateChanged(d->batteryPercent, d->isPlugged);
}

void PowerDevilDaemon::setACPlugged(bool newplugged)
{
    d->isPlugged = newplugged;
    emit stateChanged(d->batteryPercent, d->isPlugged);
}

void PowerDevilDaemon::setCurrentProfile(const QString &profile)
{
    if (!checkIfCurrentSessionActive()) {
        return;
    }

    if (profile != d->currentProfile) {
        d->currentProfile = profile;
        profileFirstLoad();
        emit profileChanged(d->currentProfile, d->availableProfiles);
    }
}

void PowerDevilDaemon::setAvailableProfiles(const QStringList &aProfiles)
{
    d->availableProfiles = aProfiles;
    emit profileChanged(d->currentProfile, d->availableProfiles);
}

bool PowerDevilDaemon::checkIfCurrentSessionActive()
{
    if (!d->ckAvailable) {
        // No way to determine if we are on the current session, simply suppose we are
        kDebug() << "Can't contact ck";
        return true;
    }

    QDBusReply<bool> rp = d->ckSessionInterface->call("IsActive");

    return rp.value();
}

void PowerDevilDaemon::setUpConsoleKit()
{
    // Let's cache the needed information to check if our session is actually active

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.ConsoleKit")) {
        // No way to determine if we are on the current session, simply suppose we are
        kDebug() << "Can't contact ck";
        d->ckAvailable = false;
        return;
    } else {
        d->ckAvailable = true;
    }

    // Otherwise, let's ask ConsoleKit
    QDBusInterface ckiface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                           "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());

    QDBusReply<QDBusObjectPath> sessionPath = ckiface.call("GetCurrentSession");

    if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
        kDebug() << "The session is not registered with ck";
        d->ckAvailable = false;
        return;
    }

    d->ckSessionInterface = new QDBusInterface("org.freedesktop.ConsoleKit", sessionPath.value().path(),
            "org.freedesktop.ConsoleKit.Session", QDBusConnection::systemBus());

    if (!d->ckSessionInterface->isValid()) {
        // As above
        kDebug() << "Can't contact iface";
        d->ckAvailable = false;
        return;
    }

    /* If everything's fine, let's attach ourself to the ActiveChanged signal.
     * You'll see we're attaching to refreshStatus without any further checks. You know why?
     * refreshStatus already checks if the console is active, so the check is already happening
     * in the underhood
     */

    QDBusConnection::systemBus().connect("org.freedesktop.ConsoleKit", sessionPath.value().path(),
                                         "org.freedesktop.ConsoleKit.Session", "ActiveChanged", this, SLOT(refreshStatus()));
}

#include "PowerDevilDaemon.moc"
