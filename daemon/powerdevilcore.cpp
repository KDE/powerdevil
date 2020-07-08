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

#include "powerdevilcore.h"

#include "PowerDevilSettings.h"

#include "powerdevilaction.h"
#include "powerdevilactionpool.h"
#include "powerdevilbackendinterface.h"
#include "powerdevilpolicyagent.h"
#include "powerdevilprofilegenerator.h"
#include "powerdevil_debug.h"

#include "actions/bundled/suspendsession.h"

#include <Solid/Battery>
#include <Solid/Device>
#include <Solid/DeviceNotifier>

#include <KAuthExecuteJob>
#include <KAuthAction>
#include <KIdleTime>
#include <KLocalizedString>
#include <KNotification>

#include <KActivities/Consumer>

#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include <QDebug>

#include <algorithm>

#ifdef Q_OS_LINUX
#include <sys/timerfd.h>
#endif

namespace PowerDevil
{

Core::Core(QObject* parent)
    : QObject(parent)
    , m_hasDualGpu(false)
    , m_backend(nullptr)
    , m_notificationsWatcher(nullptr)
    , m_criticalBatteryTimer(new QTimer(this))
    , m_activityConsumer(new KActivities::Consumer(this))
    , m_pendingWakeupEvent(true)
{
    KAuth::Action discreteGpuAction(QStringLiteral("org.kde.powerdevil.discretegpuhelper.hasdualgpu"));
    discreteGpuAction.setHelperId(QStringLiteral("org.kde.powerdevil.discretegpuhelper"));
    KAuth::ExecuteJob *discreteGpuJob = discreteGpuAction.execute();
    connect(discreteGpuJob, &KJob::result, this, [this, discreteGpuJob]  {
        if (discreteGpuJob->error()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.discretegpuhelper.hasdualgpu failed";
            qCDebug(POWERDEVIL) << discreteGpuJob->errorText();
            return;
        }
        m_hasDualGpu = discreteGpuJob->data()[QStringLiteral("hasdualgpu")].toBool();
    });
    discreteGpuJob->start();
}

Core::~Core()
{
    qCDebug(POWERDEVIL) << "Core unloading";
    // Unload all actions before exiting, and clear the cache
    ActionPool::instance()->unloadAllActiveActions();
    ActionPool::instance()->clearCache();
}

void Core::loadCore(BackendInterface* backend)
{
    if (!backend) {
        return;
    }

    m_backend = backend;

    // Async backend init - so that KDED gets a bit of a speed up
    qCDebug(POWERDEVIL) << "Core loaded, initializing backend";
    connect(m_backend, SIGNAL(backendReady()), this, SLOT(onBackendReady()));
    m_backend->init();
}

void Core::onBackendReady()
{
    qCDebug(POWERDEVIL) << "Backend ready, KDE Power Management system initialized";

    m_profilesConfig = KSharedConfig::openConfig(QStringLiteral("powermanagementprofilesrc"), KConfig::CascadeConfig);

    QStringList groups = m_profilesConfig->groupList();
    // the "migration" key is for shortcuts migration in added by migratePre512KeyboardShortcuts
    // and as such our configuration would never be considered empty, ignore it!
    groups.removeOne(QStringLiteral("migration"));

    // Is it brand new?
    if (groups.isEmpty()) {
        // Generate defaults
        qCDebug(POWERDEVIL) << "Generating a default configuration";
        bool toRam = m_backend->supportedSuspendMethods() & PowerDevil::BackendInterface::ToRam;
        bool toDisk = m_backend->supportedSuspendMethods() & PowerDevil::BackendInterface::ToDisk;
        ProfileGenerator::generateProfiles(toRam, toDisk);
        m_profilesConfig->reparseConfiguration();
    }

    // Get the battery devices ready
    {
        using namespace Solid;
        connect(DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
                this, SLOT(onDeviceAdded(QString)));
        connect(DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
                this, SLOT(onDeviceRemoved(QString)));

        // Force the addition of already existent batteries
        const auto devices = Device::listFromType(DeviceInterface::Battery, QString());
        for (const Device &device : devices) {
            onDeviceAdded(device.udi());
        }
    }

    connect(m_backend, SIGNAL(acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)),
            this, SLOT(onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)));
    connect(m_backend, SIGNAL(batteryRemainingTimeChanged(qulonglong)),
            this, SLOT(onBatteryRemainingTimeChanged(qulonglong)));
    connect(m_backend, SIGNAL(lidClosedChanged(bool)),
            this, SLOT(onLidClosedChanged(bool)));
    connect(m_backend, &BackendInterface::aboutToSuspend,
            this, &Core::onAboutToSuspend);
    connect(KIdleTime::instance(), SIGNAL(timeoutReached(int,int)),
            this, SLOT(onKIdleTimeoutReached(int,int)));
    connect(KIdleTime::instance(), SIGNAL(resumingFromIdle()),
            this, SLOT(onResumingFromIdle()));
    connect(m_activityConsumer, &KActivities::Consumer::currentActivityChanged, this, [this]() {
        loadProfile();
    });

    // Set up the policy agent
    PowerDevil::PolicyAgent::instance()->init();
    // When inhibitions change, simulate user activity, see Bug 315438
    connect(PowerDevil::PolicyAgent::instance(), &PowerDevil::PolicyAgent::unavailablePoliciesChanged, this,
            [](PowerDevil::PolicyAgent::RequiredPolicies newPolicies) {
        Q_UNUSED(newPolicies);
        KIdleTime::instance()->simulateUserActivity();
    });

    // Bug 354250: Simulate user activity when session becomes inactive,
    // this keeps us from sending the computer to sleep when switching to an idle session.
    // (this is just being lazy as it will result in us clearing everything
    connect(PowerDevil::PolicyAgent::instance(), &PowerDevil::PolicyAgent::sessionActiveChanged, this, [this](bool active) {
        if (active) {
            // force reload profile so all actions re-register their idle timeouts
            loadProfile(true /*force*/);
        } else {
            // Bug 354250: Keep us from sending the computer to sleep when switching
            // to an idle session by removing all idle timeouts
            KIdleTime::instance()->removeAllIdleTimeouts();
            m_registeredActionTimeouts.clear();
        }
    });

    // Initialize the action pool, which will also load the needed startup actions.
    PowerDevil::ActionPool::instance()->init(this);

    // Set up the critical battery timer
    m_criticalBatteryTimer->setSingleShot(true);
    m_criticalBatteryTimer->setInterval(60000);
    connect(m_criticalBatteryTimer, SIGNAL(timeout()), this, SLOT(onCriticalBatteryTimerExpired()));

    // wait until the notification system is set up before firing notifications
    // to avoid them showing ontop of ksplash...
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.freedesktop.Notifications"))) {
        onServiceRegistered(QString());
    } else {
        m_notificationsWatcher = new QDBusServiceWatcher(QStringLiteral("org.freedesktop.Notifications"),
                                                         QDBusConnection::sessionBus(),
                                                         QDBusServiceWatcher::WatchForRegistration,
                                                         this);
        connect(m_notificationsWatcher, SIGNAL(serviceRegistered(QString)), this, SLOT(onServiceRegistered(QString)));

        // ...but fire them after 30s nonetheless to ensure they've been shown
        QTimer::singleShot(30000, this, SLOT(onNotificationTimeout()));
    }

#ifdef Q_OS_LINUX

    // try creating a timerfd which can wake system from suspend
    m_timerFd = timerfd_create(CLOCK_REALTIME_ALARM, TFD_CLOEXEC);

    // if that fails due to privilges maybe, try normal timerfd
    if (m_timerFd == -1) {
        qCDebug(POWERDEVIL) << "Unable to create a CLOCK_REALTIME_ALARM timer, trying CLOCK_REALTIME\n This would mean that wakeup requests won't wake device from suspend";
        m_timerFd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
    }

    if (m_timerFd != -1) {
        m_timerFdSocketNotifier = new QSocketNotifier(m_timerFd, QSocketNotifier::Read);
        connect(m_timerFdSocketNotifier, &QSocketNotifier::activated, this, &Core::timerfdEventHandler);
        // we disable events reading for now
        m_timerFdSocketNotifier->setEnabled(false);
    } else {
        qCDebug(POWERDEVIL) << "Unable to create a CLOCK_REALTIME timer, scheduled wakeups won't be available";
    }

#endif

    // All systems up Houston, let's go!
    Q_EMIT coreReady();
    refreshStatus();
}

bool Core::isActionSupported(const QString& actionName)
{
    Action *action = ActionPool::instance()->loadAction(actionName, KConfigGroup(), this);
    if (!action) {
        return false;
    } else {
        return action->isSupported();
    }
}

void Core::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    reparseConfiguration();

    loadProfile(true);
}

void Core::reparseConfiguration()
{
    PowerDevilSettings::self()->load();
    m_profilesConfig->reparseConfiguration();

    // Config reloaded
    Q_EMIT configurationReloaded();

    // Check if critical threshold might have changed and cancel the timer if necessary.
    if (m_criticalBatteryTimer->isActive() && currentChargePercent() > PowerDevilSettings::batteryCriticalLevel()) {
        m_criticalBatteryTimer->stop();
        if (m_criticalBatteryNotification) {
            m_criticalBatteryNotification->close();
        }
    }

    if (m_lowBatteryNotification && currentChargePercent() > PowerDevilSettings::batteryLowLevel()) {
        m_lowBatteryNotification->close();
    }
}

QString Core::currentProfile() const
{
    return m_currentProfile;
}

void Core::loadProfile(bool force)
{
    QString profileId;

    // Policy check
    if (PolicyAgent::instance()->requirePolicyCheck(PolicyAgent::ChangeProfile) != PolicyAgent::None) {
        qCDebug(POWERDEVIL) << "Policy Agent prevention: on";
        return;
    }

    KConfigGroup config;

    // Check the activity in which we are in
    QString activity = m_activityConsumer->currentActivity();
    qCDebug(POWERDEVIL) << "Currently using activity " << activity;
    KConfigGroup activitiesConfig(m_profilesConfig, "Activities");
    qCDebug(POWERDEVIL) << activitiesConfig.groupList() << activitiesConfig.keyList();

    // Are we mirroring an activity?
    if (activitiesConfig.group(activity).readEntry("mode", "None") == QStringLiteral("ActLike") &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != QStringLiteral("AC") &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != QStringLiteral("Battery") &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != QStringLiteral("LowBattery")) {
        // Yes, let's use that then
        activity = activitiesConfig.group(activity).readEntry("actLike", QString());
        qCDebug(POWERDEVIL) << "Activity is a mirror";
    }

    KConfigGroup activityConfig = activitiesConfig.group(activity);
    qCDebug(POWERDEVIL) << activityConfig.groupList() << activityConfig.keyList();

    // See if this activity has priority
    if (activityConfig.readEntry("mode", "None") == QStringLiteral("SeparateSettings")) {
        // Prioritize this profile over anything
        config = activityConfig.group("SeparateSettings");
        qCDebug(POWERDEVIL) << "Activity is enforcing a different profile";
        profileId = activity;
    } else {
        // It doesn't, let's load the current state's profile
        if (m_batteriesPercent.isEmpty()) {
            qCDebug(POWERDEVIL) << "No batteries found, loading AC";
            profileId = QStringLiteral("AC");
        } else if (activityConfig.readEntry("mode", "None") == QStringLiteral("ActLike")) {
            if (activityConfig.readEntry("actLike", QString()) == QStringLiteral("AC") ||
                activityConfig.readEntry("actLike", QString()) == QStringLiteral("Battery") ||
                activityConfig.readEntry("actLike", QString()) == QStringLiteral("LowBattery")) {
                // Same as above, but with an existing profile
                config = m_profilesConfig.data()->group(activityConfig.readEntry("actLike", QString()));
                profileId = activityConfig.readEntry("actLike", QString());
                qCDebug(POWERDEVIL) << "Activity is mirroring a different profile";
            }
        } else {
            // Compute the previous and current global percentage
            const int percent = currentChargePercent();

            if (backend()->acAdapterState() == BackendInterface::Plugged) {
                profileId = QStringLiteral("AC");
                qCDebug(POWERDEVIL) << "Loading profile for plugged AC";
            } else if (percent <= PowerDevilSettings::batteryLowLevel()) {
                profileId = QStringLiteral("LowBattery");
                qCDebug(POWERDEVIL) << "Loading profile for low battery";
            } else {
                profileId = QStringLiteral("Battery");
                qCDebug(POWERDEVIL) << "Loading profile for unplugged AC";
            }
        }

        config = m_profilesConfig.data()->group(profileId);
        qCDebug(POWERDEVIL) << "Activity is not forcing a profile";
    }

    // Release any special inhibitions
    {
        QHash<QString,int>::iterator i = m_sessionActivityInhibit.begin();
        while (i != m_sessionActivityInhibit.end()) {
            PolicyAgent::instance()->ReleaseInhibition(i.value());
            i = m_sessionActivityInhibit.erase(i);
        }

        i = m_screenActivityInhibit.begin();
        while (i != m_screenActivityInhibit.end()) {
            PolicyAgent::instance()->ReleaseInhibition(i.value());
            i = m_screenActivityInhibit.erase(i);
        }
    }

    if (!config.isValid()) {
        qCWarning(POWERDEVIL) << "Profile " << profileId << "has been selected but does not exist.";
        return;
    }

    // Check: do we need to change profile at all?
    if (m_currentProfile == profileId && !force) {
        // No, let's leave things as they are
        qCDebug(POWERDEVIL) << "Skipping action reload routine as profile has not changed";

        // Do we need to force a wakeup?
        if (m_pendingWakeupEvent) {
            // Fake activity at this stage, when no timeouts are registered
            onResumingFromIdle();
            m_pendingWakeupEvent = false;
        }
    } else {
        // First of all, let's clean the old actions. This will also call the onProfileUnload callback
        ActionPool::instance()->unloadAllActiveActions();

        // Do we need to force a wakeup?
        if (m_pendingWakeupEvent) {
            // Fake activity at this stage, when no timeouts are registered
            onResumingFromIdle();
            m_pendingWakeupEvent = false;
        }

        // Cool, now let's load the needed actions
        const auto groupList = config.groupList();
        for (const QString &actionName : groupList) {
            Action *action = ActionPool::instance()->loadAction(actionName, config.group(actionName), this);
            if (action) {
                action->onProfileLoad();
            } else {
                // Ouch, error. But let's just warn and move on anyway
                //TODO Maybe Remove from the configuration if unsupported
                qCWarning(POWERDEVIL) << "The profile " << profileId <<  "tried to activate"
                                << actionName << "a non-existent action. This is usually due to an installation problem,"
                                " a configuration problem, or because the action is not supported";
            }
        }

        // We are now on a different profile
        m_currentProfile = profileId;
        Q_EMIT profileChanged(m_currentProfile);
    }

    // Now... any special behaviors we'd like to consider?
    if (activityConfig.readEntry("mode", "None") == QStringLiteral("SpecialBehavior")) {
        qCDebug(POWERDEVIL) << "Activity has special behaviors";
        KConfigGroup behaviorGroup = activityConfig.group("SpecialBehavior");
        if (behaviorGroup.readEntry("performAction", false)) {
            // Let's override the configuration for this action at all times
            ActionPool::instance()->loadAction(QStringLiteral("SuspendSession"), behaviorGroup.group("ActionConfig"), this);
            qCDebug(POWERDEVIL) << "Activity overrides suspend session action"; // debug hence not sleep
        }

        if (behaviorGroup.readEntry("noSuspend", false)) {
            qCDebug(POWERDEVIL) << "Activity triggers a suspend inhibition"; // debug hence not sleep
            // Trigger a special inhibition - if we don't have one yet
            if (!m_sessionActivityInhibit.contains(activity)) {
                int cookie =
                PolicyAgent::instance()->AddInhibition(PolicyAgent::InterruptSession, i18n("Activity Manager"),
                                                       i18n("This activity's policies prevent the system from going to sleep"));

                m_sessionActivityInhibit.insert(activity, cookie);
            }
        }

        if (behaviorGroup.readEntry("noScreenManagement", false)) {
            qCDebug(POWERDEVIL) << "Activity triggers a screen management inhibition";
            // Trigger a special inhibition - if we don't have one yet
            if (!m_screenActivityInhibit.contains(activity)) {
                int cookie =
                PolicyAgent::instance()->AddInhibition(PolicyAgent::ChangeScreenSettings, i18n("Activity Manager"),
                                                       i18n("This activity's policies prevent screen power management"));

                m_screenActivityInhibit.insert(activity, cookie);
            }
        }
    }

    // If the lid is closed, retrigger the lid close signal
    // so that "switching profile then closing the lid" has the same result as
    // "closing lid then switching profile".
    if (m_backend->isLidClosed()) {
        Q_EMIT m_backend->buttonPressed(PowerDevil::BackendInterface::LidClose);
    }
}

void Core::onDeviceAdded(const QString &udi)
{
    if (m_batteriesPercent.contains(udi) || m_peripheralBatteriesPercent.contains(udi)) {
        // We already know about this device
        return;
    }

    using namespace Solid;
    Device device(udi);
    Battery *b = qobject_cast<Battery *>(device.asDeviceInterface(DeviceInterface::Battery));

    if (!b) {
        return;
    }

    connect(b, &Battery::chargePercentChanged, this, &Core::onBatteryChargePercentChanged);
    connect(b, &Battery::chargeStateChanged, this, &Core::onBatteryChargeStateChanged);

    qCDebug(POWERDEVIL) << "Battery with UDI" << udi << "was detected";

    if (b->isPowerSupply()) {
        m_batteriesPercent[udi] = b->chargePercent();
        m_batteriesCharged[udi] = (b->chargeState() == Solid::Battery::FullyCharged);
    } else { // non-power supply batteries are treated separately
        m_peripheralBatteriesPercent[udi] = b->chargePercent();

        // notify the user about the empty mouse/keyboard when plugging it in; don't when
        // notifications aren't ready yet so we avoid showing them ontop of ksplash;
        // also we'll notify about all devices when notifications are ready anyway
        if (m_notificationsReady) {
            emitBatteryChargePercentNotification(b->chargePercent(), 1000 /* so current is always lower than previous */, udi);
        }
    }

    // If a new battery has been added, let's clear some pending suspend actions if the new global batteries percentage is
    // higher than the battery critical level. (See bug 329537)
    if (m_lowBatteryNotification && currentChargePercent() > PowerDevilSettings::batteryLowLevel()) {
        m_lowBatteryNotification->close();
    }

    if (m_criticalBatteryTimer->isActive() && currentChargePercent() > PowerDevilSettings::batteryCriticalLevel()) {
        m_criticalBatteryTimer->stop();
        if (m_criticalBatteryNotification) {
            m_criticalBatteryNotification->close();
        }
        emitRichNotification(QStringLiteral("pluggedin"),
                             i18n("Extra Battery Added"),
                             i18n("The computer will no longer go to sleep."));
    }
}

void Core::onDeviceRemoved(const QString &udi)
{
    if (!m_batteriesPercent.contains(udi) && !m_peripheralBatteriesPercent.contains(udi)) {
        // We don't know about this device
        return;
    }

    using namespace Solid;
    Device device(udi);
    Battery *b = qobject_cast<Battery *>(device.asDeviceInterface(DeviceInterface::Battery));

    disconnect(b, &Battery::chargePercentChanged, this, &Core::onBatteryChargePercentChanged);
    disconnect(b, &Battery::chargeStateChanged, this, &Core::onBatteryChargeStateChanged);

    qCDebug(POWERDEVIL) << "Battery with UDI" << udi << "has been removed";

    m_batteriesPercent.remove(udi);
    m_peripheralBatteriesPercent.remove(udi);
    m_batteriesCharged.remove(udi);
}

void Core::emitNotification(const QString &eventId, const QString &title, const QString &message, const QString &iconName)
{
    KNotification::event(eventId, title, message, iconName, nullptr, KNotification::CloseOnTimeout, QStringLiteral("powerdevil"));
}

void Core::emitRichNotification(const QString &evid, const QString &title, const QString &message)
{
    KNotification::event(evid, title, message, QPixmap(),
                         nullptr, KNotification::CloseOnTimeout, QStringLiteral("powerdevil"));
}

bool Core::emitBatteryChargePercentNotification(int currentPercent, int previousPercent, const QString &udi)
{
    using namespace Solid;
    Device device(udi);
    Battery *b = qobject_cast<Battery *>(device.asDeviceInterface(DeviceInterface::Battery));

    if (b && !b->isPowerSupply()) {
        // if you leave the device out of reach or it has not been initialized yet
        // it won't be "there" and report 0%, don't show anything in this case
        if (!b->isPresent() || b->chargePercent() == 0) {
            return false;
        }

        // Bluetooth devices don't report charge state, so it's "NoCharge" in all cases for them
        if (b->chargeState() != Battery::Discharging && b->chargeState() != Battery::NoCharge) {
            return false;
        }

        if (currentPercent <= PowerDevilSettings::peripheralBatteryLowLevel() &&
            previousPercent > PowerDevilSettings::peripheralBatteryLowLevel()) {

            QString name = device.product();
            if (!device.vendor().isEmpty()) {
                name = i18nc("%1 is vendor name, %2 is product name", "%1 %2", device.vendor(), device.product());
            }

            QString title;
            QString msg;
            QString icon;

            switch(b->type()) {
            case Battery::MouseBattery:
                title = i18n("Mouse Battery Low (%1% Remaining)", currentPercent);
                msg = i18nc("Placeholder is device name",
                            "The battery in (\"%1\") is running low, and the device may turn off at any time. "
                            "Please recharge or replace the battery.", name);
                icon = QStringLiteral("input-mouse");
                break;
            case Battery::KeyboardBattery:
                title = i18n("Keyboard Battery Low (%1% Remaining)", currentPercent);
                msg = i18nc("Placeholder is device name",
                            "The battery in (\"%1\") is running low, and the device may turn off at any time. "
                            "Please recharge or replace the battery.", name);
                icon = QStringLiteral("input-keyboard");
                break;
            case Battery::BluetoothBattery:
                title = i18n("Bluetooth Device Battery Low (%1% Remaining)", currentPercent);
                msg = i18nc("Placeholder is device name",
                            "The battery in Bluetooth device \"%1\" is running low, and the device may turn off at any time. "
                            "Please recharge or replace the battery.", name);
                icon = QStringLiteral("preferences-system-bluetooth");
                break;
            default:
                title = i18nc("The battery in an external device", "Device Battery Low (%1% Remaining)", currentPercent);
                msg = i18nc("Placeholder is device name",
                            "The battery in (\"%1\") is running low, and the device may turn off at any time. "
                            "Please recharge or replace the battery.", name);
                icon = QStringLiteral("battery-caution");
                break;
            }

            emitNotification(QStringLiteral("lowperipheralbattery"), title, msg, icon);

            return true;
        }

        return false;
    }

    if (m_backend->acAdapterState() == BackendInterface::Plugged) {
        return false;
    }

    if (currentPercent <= PowerDevilSettings::batteryCriticalLevel() &&
        previousPercent > PowerDevilSettings::batteryCriticalLevel()) {
        handleCriticalBattery(currentPercent);
        return true;
    } else if (currentPercent <= PowerDevilSettings::batteryLowLevel() &&
               previousPercent > PowerDevilSettings::batteryLowLevel()) {
        handleLowBattery(currentPercent);
        return true;
    }
    return false;
}

void Core::handleLowBattery(int percent)
{
    if (m_lowBatteryNotification) {
        return;
    }

    m_lowBatteryNotification = new KNotification(QStringLiteral("lowbattery"), KNotification::Persistent, nullptr);
    m_lowBatteryNotification->setComponentName(QStringLiteral("powerdevil"));
    m_lowBatteryNotification->setTitle(i18n("Battery Low (%1% Remaining)", percent));
    m_lowBatteryNotification->setText(i18n("Battery running low - to continue using your computer, plug it in or shut it down and change the battery."));
    m_lowBatteryNotification->setUrgency(KNotification::CriticalUrgency);
    m_lowBatteryNotification->sendEvent();
}

void Core::handleCriticalBattery(int percent)
{
    if (m_lowBatteryNotification) {
        m_lowBatteryNotification->close();
    }

    // no parent, but it won't leak, since it will be closed both in case of timeout or direct action
    m_criticalBatteryNotification = new KNotification(QStringLiteral("criticalbattery"), KNotification::Persistent, nullptr);
    m_criticalBatteryNotification->setComponentName(QStringLiteral("powerdevil"));
    m_criticalBatteryNotification->setTitle(i18n("Battery Critical (%1% Remaining)", percent));

    const QStringList actions = {i18nc("Cancel timeout that will automatically put system to sleep because of low battery", "Cancel")};

    connect(m_criticalBatteryNotification.data(), &KNotification::action1Activated, this, [this] {
        m_criticalBatteryTimer->stop();
        m_criticalBatteryNotification->close();
    });

    switch (PowerDevilSettings::batteryCriticalAction()) {
    case PowerDevil::BundledActions::SuspendSession::ShutdownMode:
        m_criticalBatteryNotification->setText(i18n("Battery level critical. Your computer will shut down in 60 seconds."));
        m_criticalBatteryNotification->setActions(actions);
        m_criticalBatteryTimer->start();
        break;
    case PowerDevil::BundledActions::SuspendSession::ToDiskMode:
        m_criticalBatteryNotification->setText(i18n("Battery level critical. Your computer will enter hibernation mode in 60 seconds."));
        m_criticalBatteryNotification->setActions(actions);
        m_criticalBatteryTimer->start();
        break;
    case PowerDevil::BundledActions::SuspendSession::ToRamMode:
        m_criticalBatteryNotification->setText(i18n("Battery level critical. Your computer will go to sleep in 60 seconds."));
        m_criticalBatteryNotification->setActions(actions);
        m_criticalBatteryTimer->start();
        break;
    default:
        m_criticalBatteryNotification->setText(i18n("Battery level critical. Please save your work."));
        // no timer, no actions
        break;
    }

    m_criticalBatteryNotification->sendEvent();
}

void Core::onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState state)
{
    qCDebug(POWERDEVIL);
    // Post request for faking an activity event - usually adapters don't plug themselves out :)
    m_pendingWakeupEvent = true;
    loadProfile();

    if (state == BackendInterface::Plugged) {
        // If the AC Adaptor has been plugged in, let's clear some pending suspend actions
        if (m_lowBatteryNotification) {
            m_lowBatteryNotification->close();
        }

        if (m_criticalBatteryTimer->isActive()) {
            m_criticalBatteryTimer->stop();
            if (m_criticalBatteryNotification) {
                m_criticalBatteryNotification->close();
            }
            emitRichNotification(QStringLiteral("pluggedin"),
                             i18n("AC Adapter Plugged In"),
                             i18n("The computer will no longer go to sleep."));
        } else {
            emitRichNotification(QStringLiteral("pluggedin"), i18n("Running on AC power"), i18n("The power adapter has been plugged in."));
        }
    } else if (state == BackendInterface::Unplugged) {
        emitRichNotification(QStringLiteral("unplugged"), i18n("Running on Battery Power"), i18n("The power adapter has been unplugged."));
    }
}

void Core::onBatteryChargePercentChanged(int percent, const QString &udi)
{
    if (m_peripheralBatteriesPercent.contains(udi)) {
        const int previousPercent = m_peripheralBatteriesPercent.value(udi);
        m_peripheralBatteriesPercent[udi] = percent;

        if (percent < previousPercent) {
            emitBatteryChargePercentNotification(percent, previousPercent, udi);
        }
        return;
    }

    // Compute the previous and current global percentage
    const int previousPercent = currentChargePercent();
    const int currentPercent = previousPercent - (m_batteriesPercent[udi] - percent);

    // Update the battery percentage
    m_batteriesPercent[udi] = percent;

    if (currentPercent < previousPercent) {
        if (emitBatteryChargePercentNotification(currentPercent, previousPercent, udi)) {
            // Only refresh status if a notification has actually been emitted
            loadProfile();
        }
    }
}

void Core::onBatteryChargeStateChanged(int state, const QString &udi)
{
    if (!m_batteriesCharged.contains(udi)) {
        return;
    }

    bool previousCharged = true;
    for (auto i = m_batteriesCharged.constBegin(); i != m_batteriesCharged.constEnd(); ++i) {
        if (!i.value()) {
            previousCharged = false;
            break;
        }
    }

    m_batteriesCharged[udi] = (state == Solid::Battery::FullyCharged);

    if (m_backend->acAdapterState() != BackendInterface::Plugged) {
        return;
    }

    bool currentCharged = true;
    for (auto i = m_batteriesCharged.constBegin(); i != m_batteriesCharged.constEnd(); ++i) {
        if (!i.value()) {
            currentCharged = false;
            break;
        }
    }

    if (!previousCharged && currentCharged) {
        emitRichNotification(QStringLiteral("fullbattery"), i18n("Charging Complete"), i18n("Battery now fully charged."));
        loadProfile();
    }
}

void Core::onCriticalBatteryTimerExpired()
{
    if (m_criticalBatteryNotification) {
        m_criticalBatteryNotification->close();
    }

    // Do that only if we're not on AC
    if (m_backend->acAdapterState() == BackendInterface::Unplugged) {
        // We consider this as a very special button
        PowerDevil::Action *helperAction = ActionPool::instance()->loadAction(QStringLiteral("HandleButtonEvents"), KConfigGroup(), this);
        if (helperAction) {
            QVariantMap args;
            args[QStringLiteral("Button")] = 32;
            args[QStringLiteral("Type")] = QVariant::fromValue<uint>(PowerDevilSettings::batteryCriticalAction());
            args[QStringLiteral("Explicit")] = true;
            helperAction->trigger(args);
        }
    }
}

void Core::onBatteryRemainingTimeChanged(qulonglong time)
{
    Q_EMIT batteryRemainingTimeChanged(time);
}

void Core::onKIdleTimeoutReached(int identifier, int msec)
{
    // Find which action(s) requested this idle timeout
    for (auto i = m_registeredActionTimeouts.constBegin(), end = m_registeredActionTimeouts.constEnd(); i != end; ++i) {
        if (i.value().contains(identifier)) {
            i.key()->onIdleTimeout(msec);

            // And it will need to be awaken
            m_pendingResumeFromIdleActions.insert(i.key());
            break;
        }
    }

    // Catch the next resume event if some actions require it
    if (!m_pendingResumeFromIdleActions.isEmpty()) {
        KIdleTime::instance()->catchNextResumeEvent();
    }
}

void Core::onLidClosedChanged(bool closed)
{
    Q_EMIT lidClosedChanged(closed);
}

void Core::onAboutToSuspend()
{
    if (PowerDevilSettings::pausePlayersOnSuspend()) {
        qCDebug(POWERDEVIL) << "Pausing all media players before sleep";

        QDBusPendingCall listNamesCall = QDBusConnection::sessionBus().interface()->asyncCall(QStringLiteral("ListNames"));
        QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(listNamesCall, this);
        connect(callWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            QDBusPendingReply<QStringList> reply = *watcher;
            watcher->deleteLater();

            if (reply.isError()) {
                qCWarning(POWERDEVIL) << "Failed to fetch list of DBus service names for pausing players on entering sleep" << reply.error().message();
                return;
            }

            const QStringList &services = reply.value();
            for (const QString &serviceName : services) {
                if (!serviceName.startsWith(QLatin1String("org.mpris.MediaPlayer2."))) {
                    continue;
                }

                qCDebug(POWERDEVIL) << "Pausing media player with service name" << serviceName;

                QDBusMessage pauseMsg = QDBusMessage::createMethodCall(serviceName,
                                                                       QStringLiteral("/org/mpris/MediaPlayer2"),
                                                                       QStringLiteral("org.mpris.MediaPlayer2.Player"),
                                                                       QStringLiteral("Pause"));
                QDBusConnection::sessionBus().asyncCall(pauseMsg);
            }
       });
    }
}

void Core::registerActionTimeout(Action* action, int timeout)
{
    // Register the timeout with KIdleTime
    int identifier = KIdleTime::instance()->addIdleTimeout(timeout);

    // Add the identifier to the action hash
    QList< int > timeouts = m_registeredActionTimeouts[action];
    timeouts.append(identifier);
    m_registeredActionTimeouts[action] = timeouts;
}

void Core::unregisterActionTimeouts(Action* action)
{
    // Clear all timeouts from the action
    const QList< int > timeoutsToClean = m_registeredActionTimeouts[action];

    for (int id : timeoutsToClean) {
        KIdleTime::instance()->removeIdleTimeout(id);
    }

    m_registeredActionTimeouts.remove(action);
}

int Core::currentChargePercent() const
{
    int chargePercent = 0;
    for (auto it = m_batteriesPercent.constBegin(); it != m_batteriesPercent.constEnd(); ++it) {
        chargePercent += it.value();
    }
    return chargePercent;
}

void Core::onResumingFromIdle()
{
    KIdleTime::instance()->simulateUserActivity();
    // Wake up the actions in which an idle action was triggered
    std::for_each(m_pendingResumeFromIdleActions.cbegin(), m_pendingResumeFromIdleActions.cend(),
        std::mem_fn(&PowerDevil::Action::onWakeupFromIdle));

    m_pendingResumeFromIdleActions.clear();
}

void Core::onNotificationTimeout()
{
    // cannot connect QTimer::singleShot directly to the other method
    onServiceRegistered(QString());
}

void Core::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service);

    if (m_notificationsReady) {
        return;
    }

    bool needsRefresh = false;

    // show warning about low batteries right on session startup, force it to show
    // by making sure the "old" percentage (that magic number) is always higher than the current one
    if (emitBatteryChargePercentNotification(currentChargePercent(), 1000)) {
        needsRefresh = true;
    }

    // now also emit notifications for all peripheral batteries
    for (auto it = m_peripheralBatteriesPercent.constBegin(), end = m_peripheralBatteriesPercent.constEnd(); it != end; ++it) {
        if (emitBatteryChargePercentNotification(it.value() /*currentPercent*/, 1000, it.key() /*udi*/)) {
            needsRefresh = true;
        }
    }

    // need to refresh status to prevent the notification from showing again when charge percentage changes
    if (needsRefresh) {
        refreshStatus();
    }

    m_notificationsReady = true;

    if (m_notificationsWatcher) {
        delete m_notificationsWatcher;
        m_notificationsWatcher = nullptr;
    }
}

BackendInterface* Core::backend()
{
    return m_backend;
}

bool Core::isLidClosed() const
{
    return m_backend->isLidClosed();
}

bool Core::isLidPresent() const
{
    return m_backend->isLidPresent();
}

bool Core::hasDualGpu() const
{
    return m_hasDualGpu;
}

uint Core::scheduleWakeup(const QString &service, const QDBusObjectPath &path, qint64 timeout)
{
    ++m_lastWakeupCookie;

    int cookie = m_lastWakeupCookie;
    // if some one is trying to time travel, deny them
    if (timeout < QDateTime::currentSecsSinceEpoch()) {
        sendErrorReply(QDBusError::InvalidArgs, "You can not schedule wakeup in past");
    } else {
#ifndef Q_OS_LINUX
        sendErrorReply(QDBusError::NotSupported, "Scheduled wakeups are available only on Linux platforms");
#else
        WakeupInfo wakeup{ service, path, cookie, timeout };
        m_scheduledWakeups << wakeup;
        qCDebug(POWERDEVIL) << "Received request to wakeup at " << QDateTime::fromSecsSinceEpoch(timeout);
        resetAndScheduleNextWakeup();
#endif
    }
    return cookie;
}

void Core::wakeup()
{
    onResumingFromIdle();
}

void Core::clearWakeup(int cookie)
{
    // if we do not have any timeouts return from here
    if (m_scheduledWakeups.isEmpty()) {
        return;
    }

    // depending on cookie, remove it from scheduled wakeups
    m_scheduledWakeups.erase(std::remove_if(m_scheduledWakeups.begin(), m_scheduledWakeups.end(), [cookie](WakeupInfo wakeup) {
        return wakeup.cookie == cookie;
    }));

    // reset timerfd
    resetAndScheduleNextWakeup();
}

qulonglong Core::batteryRemainingTime() const
{
    return m_backend->batteryRemainingTime();
}

uint Core::backendCapabilities()
{
    return m_backend->capabilities();
}

void Core::resetAndScheduleNextWakeup()
{

#ifdef Q_OS_LINUX
    // first we sort the wakeup list
    std::sort(m_scheduledWakeups.begin(), m_scheduledWakeups.end(), [](const WakeupInfo& lhs, const WakeupInfo& rhs) 
    {
        return lhs.timeout < rhs.timeout;
    });

    // we don't want any of our wakeups to repeat'
    timespec interval = {0, 0};
    timespec nextWakeup;
    bool enableNotifier = false;
    // if we don't have any wakeups left, we call it a day and stop timer_fd
    if(m_scheduledWakeups.isEmpty()) {
        nextWakeup = {0, 0};
    } else {
        // now pick the first timeout from the list
        WakeupInfo wakeup = m_scheduledWakeups.first();
        nextWakeup = {wakeup.timeout, 0};
        enableNotifier = true;
    }
    if (m_timerFd != -1) {
        const itimerspec spec = {interval, nextWakeup};
        timerfd_settime(m_timerFd, TFD_TIMER_ABSTIME, &spec, nullptr);
    }
    m_timerFdSocketNotifier->setEnabled(enableNotifier);
#endif
}

void Core::timerfdEventHandler()
{
    // wakeup from the linux/rtc

    // Disable reading events from the timer_fd
    m_timerFdSocketNotifier->setEnabled(false);

    // At this point scheduled wakeup list should not be empty, but just in case
    if (m_scheduledWakeups.isEmpty()) {
        qWarning(POWERDEVIL) << "Wakeup was recieved but list is now empty! This should not happen!";
        return;
    }

    // first thing to do is, we pick the first wakeup from list
    WakeupInfo currentWakeup = m_scheduledWakeups.takeFirst();

    // Before doing anything further, lets set the next set of wakeup alarm
    resetAndScheduleNextWakeup();

    // Now current wakeup needs to be processed
    // prepare message for sending back to the consumer
    QDBusMessage msg = QDBusMessage::createMethodCall(currentWakeup.service, currentWakeup.path.path(),
                                                      QStringLiteral("org.kde.PowerManagement"), QStringLiteral("wakeupCallback"));
    msg << currentWakeup.cookie;
    // send it away
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}

}
