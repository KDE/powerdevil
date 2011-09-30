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
#include "screensaver_interface.h"

#include "powerdevilaction.h"
#include "powerdevilactionpool.h"
#include "powerdevilbackendinterface.h"
#include "powerdevilpolicyagent.h"
#include "powerdevilprofilegenerator.h"

#include <Solid/Battery>
#include <Solid/Device>
#include <Solid/DeviceNotifier>

#include <KAction>
#include <KActionCollection>
#include <KDebug>
#include <KIdleTime>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KServiceTypeTrader>
#include <KStandardDirs>

#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

namespace PowerDevil
{

Core::Core(QObject* parent, const KComponentData &componentData)
    : QObject(parent)
    , m_backend(0)
    , m_applicationData(componentData)
    , m_criticalBatteryTimer(new QTimer(this))
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
    kDebug() << "Core loaded, initializing backend";
    connect(m_backend, SIGNAL(backendReady()), this, SLOT(onBackendReady()));
    connect(m_backend, SIGNAL(backendError(QString)), this, SLOT(onBackendError(QString)));
    m_backend->init();

    // Register DBus Metatypes
    qDBusRegisterMetaType< StringStringMap >();
}

void Core::onBackendReady()
{
    kDebug() << "Backend is ready, KDE Power Management system initialized";

    m_profilesConfig = KSharedConfig::openConfig("powerdevil2profilesrc", KConfig::SimpleConfig);

    // Is it brand new?
    if (m_profilesConfig->groupList().isEmpty()) {
        // Generate defaults
        if (ProfileGenerator::generateProfiles(true) == ProfileGenerator::ResultUpgraded) {
            // Notify the user
            emitNotification("warningnot", i18n("Your Power Profiles have been updated to be used with the new KDE Power "
                                                "Management System. You can tweak them or generate a new set of defaults from "
                                                "System Settings."), "system-software-update");
        }
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

    connect(m_backend, SIGNAL(brightnessChanged(float,PowerDevil::BackendInterface::BrightnessControlType)),
            this, SLOT(onBrightnessChanged(float)));
    connect(m_backend, SIGNAL(acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)),
            this, SLOT(onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)));
    connect(m_backend, SIGNAL(batteryRemainingTimeChanged(qulonglong)),
            this, SLOT(onBatteryRemainingTimeChanged(qulonglong)));
    connect(m_backend, SIGNAL(resumeFromSuspend()),
            this, SLOT(onResumeFromSuspend()));
    connect(KIdleTime::instance(), SIGNAL(timeoutReached(int,int)),
            this, SLOT(onKIdleTimeoutReached(int,int)));
    connect(KIdleTime::instance(), SIGNAL(resumingFromIdle()),
            this, SLOT(onResumingFromIdle()));

    // Set up the policy agent
    PowerDevil::PolicyAgent::instance()->init();

    // Initialize the action pool, which will also load the needed startup actions.
    PowerDevil::ActionPool::instance()->init(this);

    // Set up the critical battery timer
    m_criticalBatteryTimer->setSingleShot(true);
    m_criticalBatteryTimer->setInterval(30000);
    connect(m_criticalBatteryTimer, SIGNAL(timeout()), this, SLOT(onCriticalBatteryTimerExpired()));

    // In 30 seconds (so we are sure the user sees eventual notifications), check the battery state
    QTimer::singleShot(30000, this, SLOT(checkBatteryStatus()));

    // All systems up Houston, let's go!
    emit coreReady();
    refreshStatus();

    KActionCollection* actionCollection = new KActionCollection( this );

    KAction* globalAction = actionCollection->addAction("Increase Screen Brightness");
    globalAction->setText(i18nc("Global shortcut", "Increase Screen Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_MonBrightnessUp));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(increaseBrightness()));

    globalAction = actionCollection->addAction("Decrease Screen Brightness");
    globalAction->setText(i18nc("Global shortcut", "Decrease Screen Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_MonBrightnessDown));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(decreaseBrightness()));

    globalAction = actionCollection->addAction("Sleep");
    globalAction->setText(i18nc("Global shortcut", "Sleep"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_Sleep));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToRam()));

    globalAction = actionCollection->addAction("Hibernate");
    globalAction->setText(i18nc("Global shortcut", "Hibernate"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_Hibernate));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToDisk()));

    globalAction = actionCollection->addAction("PowerOff");
    //globalAction->setText(i18nc("Global shortcut", "Power Off button"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_PowerOff));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(powerOffButtonTriggered()));
}

void Core::checkBatteryStatus()
{
    // Any batteries below 50% of capacity?
    for (QHash< QString, uint >::const_iterator i = m_backend->capacities().constBegin();
         i != m_backend->capacities().constEnd(); ++i) {
        if (i.value() < 50) {
            // Notify, we have a broken battery.
            if (m_loadedBatteriesUdi.size() == 1) {
                emitNotification("brokenbattery",
                                 i18n("Your battery capacity is %1%. This means your battery is broken and "
                                      "needs a replacement. Please contact your hardware vendor for more details.",
                                      i.value()),
                                 "dialog-warning");
            } else {
                emitNotification("brokenbattery",
                                 i18n("One of your batteries (ID %2) has a capacity of %1%. This means it is broken "
                                      "and needs a replacement. Please contact your hardware vendor for more details.",
                                      i.value(), i.key()),
                                 "dialog-warning");
            }
        }
    }

    // Any recalled batteries?
    foreach (const BackendInterface::RecallNotice &notice, m_backend->recallNotices()) {
        // Notify, a battery has been recalled
        if (m_loadedBatteriesUdi.size() == 1) {
            emitNotification("brokenbattery",
                             i18n("Your battery might have been recalled by %1. Usually, when vendors recall the "
                                  "hardware, it is because of factory defects which are usually eligible for a "
                                  "free repair or substitution. Please check <a href=\"%2\">%1's website</a> to "
                                  "verify if your battery is faulted.",
                                  notice.vendor, notice.url),
                             "dialog-warning");
        } else {
            emitNotification("brokenbattery",
                             i18n("One of your batteries (ID %3) might have been recalled by %1. "
                                  "Usually, when vendors recall the hardware, it is because of factory defects "
                                  "which are usually eligible for a free repair or substitution. "
                                  "Please check <a href=\"%2\">%1's website</a> to "
                                  "verify if your battery is faulted.",
                                  notice.vendor, notice.url, notice.batteryId),
                             "dialog-warning");
        }
    }
}

void Core::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    reparseConfiguration();

    reloadProfile();
}

void Core::reloadProfile()
{
    reloadProfile(m_backend->acAdapterState());
}

void Core::reloadCurrentProfile()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    reparseConfiguration();

    loadProfile(m_currentProfile);
}

void Core::reparseConfiguration()
{
    PowerDevilSettings::self()->readConfig();
    m_profilesConfig->reparseConfiguration();

    // Config reloaded
    emit configurationReloaded();
}

StringStringMap Core::availableProfiles() const
{
    QMap< QString, QString > retmap;
    foreach (const QString &ent, m_profilesConfig->groupList()) {
        if (ent == "Performance") {
            retmap.insert(ent, i18nc("Name of a power profile", "Performance"));
        } else if (ent == "Powersave") {
            retmap.insert(ent, i18nc("Name of a power profile", "Powersave"));
        } else if (ent == "Aggressive powersave") {
            retmap.insert(ent, i18nc("Name of a power profile", "Aggressive powersave"));
        } else {
            KConfigGroup group(m_profilesConfig, ent);
            if (group.hasKey("name")) {
                retmap.insert(ent, group.readEntry("name"));
            } else {
                retmap.insert(ent, ent);
            }
        }
    }

    return retmap;
}

void Core::reloadProfile(int state)
{
    if (m_loadedBatteriesUdi.isEmpty()) {
        kDebug() << "No batteries found, loading AC";
        loadProfile(PowerDevilSettings::aCProfile());
    } else {
        // Compute the previous and current global percentage
        int percent = 0;
        for (QHash<QString,int>::const_iterator i = m_batteriesPercent.constBegin(); i != m_batteriesPercent.constEnd(); ++i) {
            percent += i.value();
        }

        if (state == BackendInterface::Plugged) {
            kDebug() << "Loading profile for plugged AC";
            loadProfile(PowerDevilSettings::aCProfile());
        } else if (percent <= PowerDevilSettings::batteryWarningLevel()) {
            loadProfile(PowerDevilSettings::warningProfile());
            kDebug() << "Loading profile for warning battery";
        } else if (percent <= PowerDevilSettings::batteryLowLevel()) {
            loadProfile(PowerDevilSettings::lowProfile());
            kDebug() << "Loading profile for low battery";
        } else {
            loadProfile(PowerDevilSettings::batteryProfile());
            kDebug() << "Loading profile for unplugged AC";
        }
    }
}

void Core::loadProfile(const QString& id)
{
    // Policy check
    if (PolicyAgent::instance()->requirePolicyCheck(PolicyAgent::ChangeProfile) != PolicyAgent::None) {
        kDebug() << "Policy Agent prevention: on";
        return;
    }

    // First of all, let's clean the old actions. This will also call the onProfileUnload callback
    ActionPool::instance()->unloadAllActiveActions();

    // Do we need to force a wakeup?
    if (m_pendingWakeupEvent) {
        // Fake activity at this stage, when no timeouts are registered
        KIdleTime::instance()->simulateUserActivity();
        m_pendingWakeupEvent = false;
    }

    // Now, let's retrieve our profile
    KConfigGroup config(m_profilesConfig, id);

    if (!config.isValid()) {
        emitNotification("powerdevilerror", i18n("The profile \"%1\" has been selected, "
                         "but it does not exist.\nPlease check your PowerDevil configuration.",
                         id), "dialog-error");
        return;
    }

    // Cool, now let's load the needed actions
    foreach (const QString &actionName, config.groupList()) {
        Action *action = ActionPool::instance()->loadAction(actionName, config.group(actionName), this);
        if (action) {
            action->onProfileLoad();
        } else {
            // Ouch, error. But let's just warn and move on anyway
            emitNotification("powerdevilerror", i18n("The profile \"%1\" tried to activate %2, "
                             "a non existent action. This is usually due to an installation problem"
                             " or to a configuration problem.",
                             id, actionName), "dialog-warning");
        }
    }

    // Set the current profile. Notify if different.
    if (m_currentProfile != id) {
        m_currentProfile = id;
        emit profileChanged(m_currentProfile);
    }

    // If the lid is closed, retrigger the lid close signal
    if (m_backend->isLidClosed()) {
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

    if (!connect(b, SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(onBatteryChargePercentChanged(int,QString)))) {
        emitNotification("powerdevilerror", i18n("Could not connect to battery interface.\n"
                         "Please check your system configuration"), "dialog-error");
    }

    kDebug() << "A new battery was detected";

    m_batteriesPercent[udi] = b->chargePercent();
    m_loadedBatteriesUdi.append(udi);
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

    kDebug() << "An existing battery has been removed";

    m_batteriesPercent.remove(udi);
    m_loadedBatteriesUdi.removeOne(udi);
}

void Core::emitNotification(const QString &evid, const QString &message, const QString &iconname)
{
    KNotification::event(evid, message, KIcon(iconname).pixmap(20, 20),
                         0, KNotification::CloseOnTimeout, m_applicationData);
}

void Core::onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState state)
{
    kDebug();
    // Post request for faking an activity event - usually adapters don't plug themselves out :)
    m_pendingWakeupEvent = true;
    reloadProfile(state);

    if (state == BackendInterface::Plugged) {
        // If the AC Adaptor has been plugged in, let's clear some pending suspend actions
        if (m_criticalBatteryTimer->isActive()) {
            m_criticalBatteryTimer->stop();
            emitNotification("criticalbattery",
                             i18n("The power adaptor has been plugged in – all pending suspend actions have been canceled."));
        } else {
            emitNotification("pluggedin", i18n("The power adaptor has been plugged in."));
        }
    } else {
        emitNotification("unplugged", i18n("The power adaptor has been unplugged."));
    }
}

void Core::onBackendError(const QString& error)
{
    emitNotification("powerdevilerror", i18n("KDE Power Management System could not be initialized. "
                         "The backend reported the following error: %1\n"
                         "Please check your system configuration", error), "dialog-error");
}

void Core::onBatteryChargePercentChanged(int percent, const QString &udi)
{
    // Compute the previous and current global percentage
    int previousPercent = 0;
    for (QHash<QString,int>::const_iterator i = m_batteriesPercent.constBegin(); i != m_batteriesPercent.constEnd(); ++i) {
        previousPercent += i.value();
    }
    int currentPercent = previousPercent - (m_batteriesPercent[udi] - percent);

    // Update the battery percentage
    m_batteriesPercent[udi] = percent;

    // And check if we need to do stuff
    if (m_backend->acAdapterState() == BackendInterface::Plugged) {
        return;
    }

    if (currentPercent <= PowerDevilSettings::batteryCriticalLevel() &&
        previousPercent > PowerDevilSettings::batteryCriticalLevel()) {
        switch (PowerDevilSettings::batteryCriticalAction()) {
        case 3:
            emitNotification("criticalbattery",
                             i18n("Your battery level is critical, the computer will be halted in 30 seconds."),
                             "dialog-warning");
            m_criticalBatteryTimer->start();
            break;
        case 2:
            emitNotification("criticalbattery",
                             i18n("Your battery level is critical, the computer will be hibernated in 30 seconds."),
                             "dialog-warning");
            m_criticalBatteryTimer->start();
            break;
        case 1:
            emitNotification("criticalbattery",
                             i18n("Your battery level is critical, the computer will be suspended in 30 seconds."),
                             "dialog-warning");
            m_criticalBatteryTimer->start();
            break;
        default:
            emitNotification("criticalbattery",
                             i18n("Your battery level is critical: save your work as soon as possible."),
                             "dialog-warning");
            break;
        }
    } else if (currentPercent <= PowerDevilSettings::batteryWarningLevel() &&
               previousPercent > PowerDevilSettings::batteryWarningLevel()) {
        emitNotification("warningbattery", i18n("Your battery has reached the warning level."),
                         "dialog-warning");
        refreshStatus();
    } else if (currentPercent <= PowerDevilSettings::batteryLowLevel() &&
               previousPercent > PowerDevilSettings::batteryLowLevel()) {
        emitNotification("lowbattery", i18n("Your battery has reached a low level."),
                         "dialog-warning");
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

void Core::onBrightnessChanged(float brightness)
{
    emit brightnessChanged(brightness);
}

void Core::onResumeFromSuspend()
{
    // Notify the screensaver
    OrgFreedesktopScreenSaverInterface iface("org.freedesktop.ScreenSaver",
                                             "/ScreenSaver",
                                             QDBusConnection::sessionBus());
    iface.SimulateUserActivity();

    emit resumingFromSuspend();
}

void Core::onKIdleTimeoutReached(int identifier, int msec)
{
    // Find which action(s) requested this idle timeout
    for (QHash< Action*, QList< int > >::const_iterator i = m_registeredActionTimeouts.constBegin();
         i != m_registeredActionTimeouts.constEnd(); ++i) {
        foreach (int timeoutId, i.value()) {
            if (timeoutId == identifier) {
                // Yep.
                i.key()->onIdleTimeout(msec);
                // And it will need to be awaken
                if (!m_pendingResumeFromIdleActions.contains(i.key())) {
                    m_pendingResumeFromIdleActions.append(i.key());
                }
            }
        }
    }

    // Catch the next resume event if some actions require it
    if (!m_pendingResumeFromIdleActions.isEmpty()) {
        KIdleTime::instance()->catchNextResumeEvent();
    }
}

void Core::registerActionTimeout(Action* action, int timeout)
{
    int identifier = -1;
    // Are there any registered timeouts with the same value?
    if (m_registeredIdleTimeouts.contains(timeout)) {
        // Easy game
        identifier = m_registeredIdleTimeouts[timeout];
    } else {
        // Register the timeout with KIdleTime
        identifier = KIdleTime::instance()->addIdleTimeout(timeout);
        // And add it to the hash
        m_registeredIdleTimeouts.insert(timeout, identifier);
    }

    // Add the identifier to the action hash
    QList< int > timeouts = m_registeredActionTimeouts[action];
    timeouts.append(identifier);
    m_registeredActionTimeouts[action] = timeouts;
}

void Core::unregisterActionTimeouts(Action* action)
{
    // Clear all timeouts from the action: if the timeouts are not used anywhere else, just remove
    // them from KIdleTime as well.
    QList< int > timeoutsToClean = m_registeredActionTimeouts[action];
    m_registeredActionTimeouts.remove(action);
    for (QHash< Action*, QList< int > >::const_iterator i = m_registeredActionTimeouts.constBegin();
        i != m_registeredActionTimeouts.constEnd(); ++i) {
        foreach (int timeoutId, i.value()) {
            if (timeoutsToClean.contains(timeoutId)) {
                timeoutsToClean.removeOne(timeoutId);
            }
        }
    }

    // Clean the remaining ones
    foreach (int id, timeoutsToClean) {
        KIdleTime::instance()->removeIdleTimeout(id);
        m_registeredIdleTimeouts.remove(m_registeredIdleTimeouts.key(id));
    }
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

void Core::increaseBrightness()
{
    m_backend->brightnessKeyPressed(BackendInterface::Increase);
}

void Core::decreaseBrightness()
{
    m_backend->brightnessKeyPressed(BackendInterface::Decrease);
}

BackendInterface* Core::backend()
{
    return m_backend;
}

void Core::suspendHybrid()
{
    triggerSuspendSession(4);
}

void Core::suspendToDisk()
{
    triggerSuspendSession(2);
}

void Core::suspendToRam()
{
    triggerSuspendSession(1);
}

qulonglong Core::batteryRemainingTime() const
{
    return m_backend->batteryRemainingTime();
}

int Core::brightness() const
{
    return m_backend->brightness();
}

QString Core::currentProfile() const
{
    return m_currentProfile;
}

uint Core::backendCapabilities()
{
    return m_backend->capabilities();
}

void Core::setBrightness(int percent)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<float>((float)percent);
    args["Explicit"] = true;
    ActionPool::instance()->loadAction("BrightnessControl", KConfigGroup(), this)->trigger(args);
}

void Core::triggerSuspendSession(uint action)
{
    PowerDevil::Action *helperAction = ActionPool::instance()->loadAction("SuspendSession", KConfigGroup(), this);
    if (helperAction) {
        QVariantMap args;
        args["Type"] = action;
        args["Explicit"] = true;
        helperAction->trigger(args);
    }
}

void Core::powerOffButtonTriggered()
{
    emit m_backend->buttonPressed(PowerDevil::BackendInterface::PowerButton);
}

}

#include "powerdevilcore.moc"
