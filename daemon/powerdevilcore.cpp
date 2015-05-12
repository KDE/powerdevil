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
#include <Solid/Power/PowerManagement>

#include <KIdleTime>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KServiceTypeTrader>

#include <KActivities/Consumer>

#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>

#include <QDebug>

namespace PowerDevil
{

Core::Core(QObject* parent)
    : QObject(parent)
    , m_backend(nullptr)
    , m_notificationsWatcher(nullptr)
    , m_criticalBatteryTimer(new QTimer(this))
    , m_activityConsumer(new KActivities::Consumer(this))
    , m_pendingWakeupEvent(true)
{
}

Core::~Core()
{
    // Unload all actions before exiting, and clear the cache
    ActionPool::instance()->unloadAllActiveActions();
    ActionPool::instance()->clearCache();
}

void Core::loadCore(BackendInterface* backend)
{
    if (!backend) {
        onBackendError(i18n("No valid Power Management backend plugins are available. "
                            "A new installation might solve this problem."));
        return;
    }

    m_backend = backend;

    // Async backend init - so that KDED gets a bit of a speed up
    qCDebug(POWERDEVIL) << "Core loaded, initializing backend";
    connect(m_backend, SIGNAL(backendReady()), this, SLOT(onBackendReady()));
    connect(m_backend, SIGNAL(backendError(QString)), this, SLOT(onBackendError(QString)));
    m_backend->init();
}

void Core::onBackendReady()
{
    qCDebug(POWERDEVIL) << "Backend is ready, KDE Power Management system initialized";

    m_profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::CascadeConfig);

    // Is it brand new?
    if (m_profilesConfig->groupList().isEmpty()) {
        // Generate defaults
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
        foreach (const Device &device, Device::listFromType(DeviceInterface::Battery, QString())) {
            onDeviceAdded(device.udi());
        }
    }

    connect(m_backend, SIGNAL(acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)),
            this, SLOT(onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)));
    connect(m_backend, SIGNAL(batteryRemainingTimeChanged(qulonglong)),
            this, SLOT(onBatteryRemainingTimeChanged(qulonglong)));
    connect(KIdleTime::instance(), SIGNAL(timeoutReached(int,int)),
            this, SLOT(onKIdleTimeoutReached(int,int)));
    connect(KIdleTime::instance(), SIGNAL(resumingFromIdle()),
            this, SLOT(onResumingFromIdle()));
    connect(m_activityConsumer, &KActivities::Consumer::currentActivityChanged, this, [this]() {
        loadProfile();
    });

    // Set up the policy agent
    PowerDevil::PolicyAgent::instance()->init();

    // Initialize the action pool, which will also load the needed startup actions.
    PowerDevil::ActionPool::instance()->init(this);

    // Set up the critical battery timer
    m_criticalBatteryTimer->setSingleShot(true);
    m_criticalBatteryTimer->setInterval(60000);
    connect(m_criticalBatteryTimer, SIGNAL(timeout()), this, SLOT(onCriticalBatteryTimerExpired()));

    // wait until the notification system is set up before firing notifications
    // to avoid them showing ontop of ksplash...
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.Notifications")) {
        onServiceRegistered(QString());
    } else {
        m_notificationsWatcher = new QDBusServiceWatcher("org.freedesktop.Notifications",
                                                         QDBusConnection::sessionBus(),
                                                         QDBusServiceWatcher::WatchForRegistration,
                                                         this);
        connect(m_notificationsWatcher, SIGNAL(serviceRegistered(QString)), this, SLOT(onServiceRegistered(QString)));

        // ...but fire them after 30s nonetheless to ensure they've been shown
        QTimer::singleShot(30000, this, SLOT(onNotificationTimeout()));
    }

    // All systems up Houston, let's go!
    emit coreReady();
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

QString Core::checkBatteryStatus(bool notify)
{
    QString lastMessage;
    // Any batteries below 50% of capacity?
    for (QHash< QString, uint >::const_iterator i = m_backend->capacities().constBegin();
         i != m_backend->capacities().constEnd(); ++i) {
        if (i.value() < 50) {
            // Notify, we have a broken battery.
            if (m_loadedBatteriesUdi.size() == 1) {
                lastMessage = i18n("Your battery capacity is %1%. This means your battery is broken and "
                                   "needs a replacement. Please contact your hardware vendor for more details.",
                                   i.value());
            } else {
                lastMessage = i18n("One of your batteries (ID %2) has a capacity of %1%. This means it is broken "
                                   "and needs a replacement. Please contact your hardware vendor for more details.",
                                   i.value(), i.key());
            }
            if (notify) {
                emitRichNotification("brokenbattery", i18n("Broken Battery"), lastMessage);
            }
        }
    }

    return lastMessage;
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
    emit configurationReloaded();
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
    qCDebug(POWERDEVIL) << "We are now into activity " << activity;
    KConfigGroup activitiesConfig(m_profilesConfig, "Activities");
    qCDebug(POWERDEVIL) << activitiesConfig.groupList() << activitiesConfig.keyList();

    // Are we mirroring an activity?
    if (activitiesConfig.group(activity).readEntry("mode", "None") == "ActLike" &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != "AC" &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != "Battery" &&
        activitiesConfig.group(activity).readEntry("actLike", QString()) != "LowBattery") {
        // Yes, let's use that then
        activity = activitiesConfig.group(activity).readEntry("actLike", QString());
        qCDebug(POWERDEVIL) << "Activity is a mirror";
    }

    KConfigGroup activityConfig = activitiesConfig.group(activity);
    qCDebug(POWERDEVIL) << activityConfig.groupList() << activityConfig.keyList();

    // See if this activity has priority
    if (activityConfig.readEntry("mode", "None") == "SeparateSettings") {
        // Prioritize this profile over anything
        config = activityConfig.group("SeparateSettings");
        qCDebug(POWERDEVIL) << "Activity is enforcing a different profile";
        profileId = activity;
    } else if (activityConfig.readEntry("mode", "None") == "ActLike") {
        if (activityConfig.readEntry("actLike", QString()) == "AC" ||
            activityConfig.readEntry("actLike", QString()) == "Battery" ||
            activityConfig.readEntry("actLike", QString()) == "LowBattery") {
            // Same as above, but with an existing profile
            config = m_profilesConfig.data()->group(activityConfig.readEntry("actLike", QString()));
            profileId = activityConfig.readEntry("actLike", QString());
            qCDebug(POWERDEVIL) << "Activity is mirroring a different profile";
        }
    } else {
        // It doesn't, let's load the current state's profile
        if (m_loadedBatteriesUdi.isEmpty()) {
            qCDebug(POWERDEVIL) << "No batteries found, loading AC";
            profileId = "AC";
        } else {
            // Compute the previous and current global percentage
            const int percent = currentChargePercent();

            if (backend()->acAdapterState() == BackendInterface::Plugged) {
                profileId = "AC";
                qCDebug(POWERDEVIL) << "Loading profile for plugged AC";
            } else if (percent <= PowerDevilSettings::batteryLowLevel()) {
                profileId = "LowBattery";
                qCDebug(POWERDEVIL) << "Loading profile for low battery";
            } else {
                profileId = "Battery";
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
        emitNotification("powerdevilerror", i18n("The profile \"%1\" has been selected, "
                         "but it does not exist.\nPlease check your PowerDevil configuration.",
                         profileId));
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
            KIdleTime::instance()->simulateUserActivity();
            m_pendingWakeupEvent = false;
        }
    } else {
        // First of all, let's clean the old actions. This will also call the onProfileUnload callback
        ActionPool::instance()->unloadAllActiveActions();

        // Do we need to force a wakeup?
        if (m_pendingWakeupEvent) {
            // Fake activity at this stage, when no timeouts are registered
            onResumingFromIdle();
            KIdleTime::instance()->simulateUserActivity();
            m_pendingWakeupEvent = false;
        }

        // Cool, now let's load the needed actions
        foreach (const QString &actionName, config.groupList()) {
            Action *action = ActionPool::instance()->loadAction(actionName, config.group(actionName), this);
            if (action) {
                action->onProfileLoad();
            } else {
                // Ouch, error. But let's just warn and move on anyway
                //TODO Maybe Remove from the configuration if unsupported
                qCWarning(POWERDEVIL) << "The profile " << profileId <<  "tried to activate"
                                << actionName << "a non-existent action. This is usually due to an installation problem,"
                                " or to a configuration problem, or simply the action is not supported";
            }
        }

        // We are now on a different profile
        m_currentProfile = profileId;
        emit profileChanged(m_currentProfile);
    }

    // Now... any special behaviors we'd like to consider?
    if (activityConfig.readEntry("mode", "None") == "SpecialBehavior") {
        qCDebug(POWERDEVIL) << "Activity has special behaviors";
        KConfigGroup behaviorGroup = activityConfig.group("SpecialBehavior");
        if (behaviorGroup.readEntry("performAction", false)) {
            // Let's override the configuration for this action at all times
            ActionPool::instance()->loadAction("SuspendSession", behaviorGroup.group("ActionConfig"), this);
            qCDebug(POWERDEVIL) << "Activity overrides suspend session action";
        }

        if (behaviorGroup.readEntry("noSuspend", false)) {
            qCDebug(POWERDEVIL) << "Activity triggers a suspend inhibition";
            // Trigger a special inhibition - if we don't have one yet
            if (!m_sessionActivityInhibit.contains(activity)) {
                int cookie =
                PolicyAgent::instance()->AddInhibition(PolicyAgent::InterruptSession, i18n("Activity Manager"),
                                                       i18n("This activity's policies prevent the system from suspending"));

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
    if (isLidClosed()) {
        emit m_backend->buttonPressed(PowerDevil::BackendInterface::LidClose);
    }
}

void Core::onDeviceAdded(const QString& udi)
{
    if (m_loadedBatteriesUdi.contains(udi)) {
        // We already know about this device
        return;
    }

    using namespace Solid;
    Device device(udi);
    Battery *b = qobject_cast<Battery*>(device.asDeviceInterface(DeviceInterface::Battery));

    if (!b) {
        // Not interesting to us
        return;
    }

    if (b->type() != Solid::Battery::PrimaryBattery && b->type() != Solid::Battery::UpsBattery) {
        // Not interesting to us
        return;
    }

    if (!b->isPowerSupply()) {
        // TODO: At some later point it would be really nice to handle those batteries too
        // eg, show "your mouse is running low", but in the mean time, we don't care about those
        return;
    }

    if (!connect(b, SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(onBatteryChargePercentChanged(int,QString))) ||
        !connect(b, SIGNAL(chargeStateChanged(int,QString)),
                 this, SLOT(onBatteryChargeStateChanged(int,QString)))) {
        emitNotification("powerdevilerror", i18n("Could not connect to battery interface.\n"
                         "Please check your system configuration"));
    }

    qCDebug(POWERDEVIL) << "A new battery was detected";

    m_batteriesPercent[udi] = b->chargePercent();
    if (b->chargeState() == Solid::Battery::NoCharge) {
      m_batteriesCharged[udi] = true;
    } else {
      m_batteriesCharged[udi] = false;
    }
    m_loadedBatteriesUdi.append(udi);

    const int chargePercent = currentChargePercent();

    // If a new battery has been added, let's clear some pending suspend actions if the new global batteries percentage is
    // higher than the battery critical level. (See bug 329537)
    if (m_criticalBatteryTimer->isActive() && chargePercent > PowerDevilSettings::batteryCriticalLevel()) {
        m_criticalBatteryTimer->stop();
        emitRichNotification("criticalbattery",
                             i18n("Additional Battery Added"),
                             i18n("All pending suspend actions have been canceled."));
    }
}

void Core::onDeviceRemoved(const QString& udi)
{
    if (!m_loadedBatteriesUdi.contains(udi)) {
        // We don't know about this device
        return;
    }

    using namespace Solid;
    Device device(udi);
    Battery *b = qobject_cast<Battery*>(device.asDeviceInterface(DeviceInterface::Battery));

    disconnect(b, SIGNAL(chargePercentChanged(int,QString)),
               this, SLOT(onBatteryChargePercentChanged(int,QString)));
    disconnect(b, SIGNAL(chargeStateChanged(int,QString)),
               this, SLOT(onBatteryChargeStateChanged(int,QString)));

    qCDebug(POWERDEVIL) << "An existing battery has been removed";

    m_batteriesPercent.remove(udi);
    m_batteriesCharged.remove(udi);
    m_loadedBatteriesUdi.removeOne(udi);
}

void Core::emitNotification(const QString &evid, const QString &message, const QString &iconname)
{
    if (!iconname.isEmpty()) {
      KNotification::event(evid, message, QIcon::fromTheme(iconname).pixmap(48,48),
                           0, KNotification::CloseOnTimeout, "powerdevil");
    } else {
      KNotification::event(evid, message, QPixmap(),
                           0, KNotification::CloseOnTimeout, "powerdevil");
    }
}

void Core::emitRichNotification(const QString &evid, const QString &title, const QString &message)
{
    KNotification::event(evid, title, message, QPixmap(),
                         0, KNotification::CloseOnTimeout, "powerdevil");
}

bool Core::emitBatteryChargePercentNotification(int currentPercent, int previousPercent)
{
    if (m_backend->acAdapterState() == BackendInterface::Plugged) {
        return false;
    }

    if (currentPercent <= PowerDevilSettings::batteryCriticalLevel() &&
        previousPercent > PowerDevilSettings::batteryCriticalLevel()) {
        switch (PowerDevilSettings::batteryCriticalAction()) {
        case PowerDevil::BundledActions::SuspendSession::ShutdownMode:
            emitRichNotification("criticalbattery", i18n("Battery Critical (%1% Remaining)", currentPercent),
                             i18n("Your battery level is critical, the computer will be halted in 60 seconds."));
            m_criticalBatteryTimer->start();
            break;
        case PowerDevil::BundledActions::SuspendSession::ToDiskMode:
            emitRichNotification("criticalbattery", i18n("Battery Critical (%1% Remaining)", currentPercent),
                             i18n("Your battery level is critical, the computer will be hibernated in 60 seconds."));
            m_criticalBatteryTimer->start();
            break;
        case PowerDevil::BundledActions::SuspendSession::ToRamMode:
            emitRichNotification("criticalbattery", i18n("Battery Critical (%1% Remaining)", currentPercent),
                             i18n("Your battery level is critical, the computer will be suspended in 60 seconds."));
            m_criticalBatteryTimer->start();
            break;
        default:
            emitRichNotification("criticalbattery", i18n("Battery Critical (%1% Remaining)", currentPercent),
                                 i18n("Your battery level is critical, save your work as soon as possible."));
            break;
        }
        return true;
    } else if (currentPercent <= PowerDevilSettings::batteryLowLevel() &&
               previousPercent > PowerDevilSettings::batteryLowLevel()) {
        emitRichNotification("lowbattery", i18n("Battery Low (%1% Remaining)", currentPercent),
                             i18n("Your battery is low. If you need to continue using your computer, either plug in your computer, or shut it down and then change the battery."));
        return true;
    }
    return false;
}


void Core::onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState state)
{
    qCDebug(POWERDEVIL);
    // Post request for faking an activity event - usually adapters don't plug themselves out :)
    m_pendingWakeupEvent = true;
    loadProfile();

    if (state == BackendInterface::Plugged) {
        // If the AC Adaptor has been plugged in, let's clear some pending suspend actions
        if (m_criticalBatteryTimer->isActive()) {
            m_criticalBatteryTimer->stop();
            emitRichNotification("criticalbattery",
                             i18n("AC Adapter Plugged In"),
                             i18n("All pending suspend actions have been canceled."));
        } else {
            emitRichNotification("pluggedin", i18n("Running on AC power"), i18n("The power adaptor has been plugged in."));
        }
    } else if (state == BackendInterface::Unplugged) {
        emitRichNotification("unplugged", i18n("Running on Battery Power"), i18n("The power adaptor has been unplugged."));
    }
}

void Core::onBackendError(const QString& error)
{
    emitNotification("powerdevilerror", i18n("KDE Power Management System could not be initialized. "
                         "The backend reported the following error: %1\n"
                         "Please check your system configuration", error));
}

void Core::onBatteryChargePercentChanged(int percent, const QString &udi)
{
    // Compute the previous and current global percentage
    const int previousPercent = currentChargePercent();
    const int currentPercent = previousPercent - (m_batteriesPercent[udi] - percent);

    // Update the battery percentage
    m_batteriesPercent[udi] = percent;

    if (currentPercent < previousPercent) {
        if (emitBatteryChargePercentNotification(currentPercent, previousPercent)) {
            // Only refresh status if a notification has actually been emitted
            refreshStatus();
        }
    }
}

void Core::onBatteryChargeStateChanged(int state, const QString &udi)
{
    bool previousCharged = true;
    for (QHash<QString,bool>::const_iterator i = m_batteriesCharged.constBegin(); i != m_batteriesCharged.constEnd(); ++i) {
        if (!i.value()) {
            previousCharged = false;
            break;
        }
    }

    m_batteriesCharged[udi] = (state == Solid::Battery::NoCharge);

    if (m_backend->acAdapterState() != BackendInterface::Plugged) {
        return;
    }

    bool currentCharged = true;
    for (QHash<QString,bool>::const_iterator i = m_batteriesCharged.constBegin(); i != m_batteriesCharged.constEnd(); ++i) {
        if (!i.value()) {
            currentCharged = false;
            break;
        }
    }

    if (!previousCharged && currentCharged) {
        emitRichNotification("fullbattery", i18n("Charge Complete"), i18n("Your battery is now fully charged."));
        refreshStatus();
    }
}

void Core::onCriticalBatteryTimerExpired()
{
    // Do that only if we're not on AC
    if (m_backend->acAdapterState() == BackendInterface::Unplugged) {
        // We consider this as a very special button
        PowerDevil::Action *helperAction = ActionPool::instance()->loadAction("HandleButtonEvents", KConfigGroup(), this);
        if (helperAction) {
            QVariantMap args;
            args["Button"] = 32;
            args["Type"] = QVariant::fromValue<uint>(PowerDevilSettings::batteryCriticalAction());
            helperAction->trigger(args);
        }
    }
}

void Core::onBatteryRemainingTimeChanged(qulonglong time)
{
    emit batteryRemainingTimeChanged(time);
}

void Core::onKIdleTimeoutReached(int identifier, int msec)
{
    // Find which action(s) requested this idle timeout
    for (QHash< Action*, QList< int > >::const_iterator i = m_registeredActionTimeouts.constBegin();
         i != m_registeredActionTimeouts.constEnd(); ++i) {
        if (i.value().contains(identifier)) {
            i.key()->onIdleTimeout(msec);
            // And it will need to be awaken
            if (!m_pendingResumeFromIdleActions.contains(i.key())) {
                m_pendingResumeFromIdleActions.append(i.key());
            }
            break;
        }
    }

    // Catch the next resume event if some actions require it
    if (!m_pendingResumeFromIdleActions.isEmpty()) {
        KIdleTime::instance()->catchNextResumeEvent();
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
    QList< int > timeoutsToClean = m_registeredActionTimeouts[action];

    foreach (int id, timeoutsToClean) {
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
    // Wake up the actions in which an idle action was triggered
    QList< Action* >::iterator i = m_pendingResumeFromIdleActions.begin();
    while (i != m_pendingResumeFromIdleActions.end()) {
        (*i)->onWakeupFromIdle();
        i = m_pendingResumeFromIdleActions.erase(i);
    }
}

void Core::onNotificationTimeout()
{
    // cannot connect QTimer::singleShot directly to the other method
    onServiceRegistered(QString());
}

void Core::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service);

    static bool notificationsReady = false;
    if (notificationsReady) {
        return;
    }

    // Now emit all the notifications
    checkBatteryStatus();

    // show warning about low batteries right on session startup, force it to show
    // by making sure the "old" percentage (that magic number) is always higher than the current one
    if (emitBatteryChargePercentNotification(currentChargePercent(), 1000)) {
        // need to refresh status to prevent the notification from showing again when charge percentage changes
        refreshStatus();
    }

    notificationsReady = true;

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
    return Solid::PowerManagement::isLidClosed();
}

bool Core::isLidPresent() const
{
    return Solid::PowerManagement::hasLid();
}

qulonglong Core::batteryRemainingTime() const
{
    return m_backend->batteryRemainingTime();
}

uint Core::backendCapabilities()
{
    return m_backend->capabilities();
}

}
