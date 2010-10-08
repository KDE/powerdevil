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

#include <QtDBus/QDBusConnection>
#include <KMessageBox>
#include "powerdevilbackendinterface.h"
#include <Solid/Device>
#include <solid/battery.h>
#include <KIdleTime>
#include "PowerDevilSettings.h"
#include <KDebug>
#include <KActionCollection>
#include <KAction>
#include <KNotification>
#include "powerdevilactionpool.h"
#include "powerdevilaction.h"
#include <Solid/DeviceNotifier>
#include <powerdevilpolicyagent.h>
#include <QDBusConnectionInterface>
#include <KLocalizedString>
#include <KServiceTypeTrader>
#include "powermanagementadaptor.h"
#include <KDirWatch>
#include <KStandardDirs>

namespace PowerDevil
{

Core::Core(QObject* parent, const KComponentData &componentData)
    : QObject(parent)
    , m_applicationData(componentData)
{
    // Before doing anything, let's set up our backend
    KService::List offers = KServiceTypeTrader::self()->query("PowerDevilBackend", "(Type == 'Service')");

    foreach (const KService::Ptr &ptr, offers) {
        QString error_string;
        m_backend = ptr->createInstance<PowerDevil::BackendInterface>(0, QVariantList(), &error_string);

        if (!m_backend != 0) {
            kDebug() << "Error loading '" << ptr->name() << "', KService said: " << error_string;
        }
    }

    // Async backend init - so that KDED gets a bit of a speed up
    connect(m_backend, SIGNAL(backendReady()), this, SLOT(onBackendReady()));
    connect(m_backend, SIGNAL(backendError(QString)), this, SLOT(onBackendError(QString)));
    m_backend->init();
}

Core::~Core()
{

}

void Core::onBackendReady()
{
    kDebug() << "Backend is ready, PowerDevil system initialized";
    QDBusConnection conn = QDBusConnection::systemBus();

    if (conn.interface()->isServiceRegistered("org.freedesktop.PowerManagement") ||
            conn.interface()->isServiceRegistered("com.novell.powersave") ||
            conn.interface()->isServiceRegistered("org.freedesktop.Policy.Power")) {
        kError() << "PowerDevil not initialized, another power manager has been detected";
        return;
    }

    m_profilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);
//     setAvailableProfiles(m_profilesConfig->groupList());

    // Get the battery devices ready
    {
        using namespace Solid;
        connect(DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
                this, SLOT(onDeviceAdded(QString)));
        connect(DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
                this, SLOT(onDeviceRemoved(QString)));

        // Force the addition of already existent batteries
        foreach(const Device &device, Device::listFromType(DeviceInterface::Battery, QString())) {
            onDeviceAdded(device.udi());
        }
    }

    connect(m_backend, SIGNAL(brightnessChanged(float,PowerDevil::BackendInterface::BrightnessControlType)),
            this, SLOT(onBrightnessChanged(float)));
    connect(m_backend, SIGNAL(acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)),
            this, SLOT(onAcAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState)));
    connect(m_backend, SIGNAL(buttonPressed(PowerDevil::BackendInterface::ButtonType)),
            this, SLOT(onButtonPressed(PowerDevil::BackendInterface::ButtonType)));
    connect(m_backend, SIGNAL(batteryRemainingTimeChanged(int)),
            this, SLOT(onBatteryRemainingTimeChanged(int)));
    connect(KIdleTime::instance(), SIGNAL(timeoutReached(int,int)),
            this, SLOT(onKIdleTimeoutReached(int,int)));
    connect(KIdleTime::instance(), SIGNAL(resumingFromIdle()),
            this, SLOT(onResumingFromIdle()));

    // Listen to profile changes
    KDirWatch *profilesWatch = new KDirWatch(this);
    profilesWatch->addFile(KStandardDirs::locate("config", "powerdevilprofilesrc"));
    connect(profilesWatch, SIGNAL(dirty(QString)), this, SLOT(reloadCurrentProfile()));
    connect(profilesWatch, SIGNAL(created(QString)), this, SLOT(reloadCurrentProfile()));
    connect(profilesWatch, SIGNAL(deleted(QString)), this, SLOT(reloadCurrentProfile()));

    //DBus
    new PowerManagementAdaptor(this);
    QDBusConnection c = QDBusConnection::sessionBus();

    c.registerService("org.kde.Solid.PowerManagement");
    c.registerObject("/org/kde/Solid/PowerManagement", this);

    conn.interface()->registerService("org.freedesktop.Policy.Power");

    // Set up the policy agent
    PowerDevil::PolicyAgent::instance()->init();

    // All systems up Houston, let's go!
    refreshStatus();

    KActionCollection* actionCollection = new KActionCollection( this );

    KAction* globalAction = actionCollection->addAction("Increase Screen Brightness");
    globalAction->setText(i18nc("Global shortcut", "Increase Screen Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_MonBrightnessUp),
                                    KAction::ShortcutTypes(KAction::ActiveShortcut | KAction::DefaultShortcut),
                                    KAction::NoAutoloading);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(increaseBrightness()));

    globalAction = actionCollection->addAction("Decrease Screen Brightness");
    globalAction->setText(i18nc("Global shortcut", "Decrease Screen Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_MonBrightnessDown),
                                    KAction::ShortcutTypes(KAction::ActiveShortcut | KAction::DefaultShortcut),
                                    KAction::NoAutoloading);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(decreaseBrightness()));
}

void Core::refreshStatus()
{
    /* The configuration could have changed if this function was called, so
     * let's resync it.
     */
    PowerDevilSettings::self()->readConfig();
    m_profilesConfig->reparseConfiguration();

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
    PowerDevilSettings::self()->readConfig();
    m_profilesConfig->reparseConfiguration();
    loadProfile(m_currentProfile);
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

void Core::loadProfile(const QString& name)
{
    // Policy check
    if (!PolicyAgent::instance()->canChangeProfile()) {
        kDebug() << "Policy Agent prevention: on";
        return;
    }

    // First of all, let's clean the old actions. This will also call the onProfileUnload callback
    ActionPool::instance()->unloadAllActiveActions();

    // Now, let's retrieve our profile
    KConfigGroup config(m_profilesConfig, name);

    if (!config.isValid() || config.groupList().isEmpty()) {
        emitNotification("powerdevilerror", i18n("The profile \"%1\" has been selected, "
                         "but it does not exist.\nPlease check your PowerDevil configuration.",
                         name), "dialog-error");
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
                             name, actionName), "dialog-warning");          
        }        
    }

    // And set the current profile
    m_currentProfile = name;
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

    if (!b) {
        // Not interesting to us
        return;
    }

    if (b->type() != Solid::Battery::PrimaryBattery) {
        // Not interesting to us
        return;
    }

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
    reloadProfile(state);

    if (state == BackendInterface::Plugged) {
        // If the AC Adaptor has been plugged in, let's clear some pending suspend actions
        emitNotification("pluggedin", i18n("The power adaptor has been plugged in."));
    } else {
        emitNotification("unplugged", i18n("The power adaptor has been unplugged."));
    }
}

void Core::onBackendError(const QString& error)
{

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
//         switch (PowerDevilSettings::batLowAction()) {
//         case Shutdown:
//             emitNotification("criticalbattery",
//                                  i18np("Your battery level is critical, the computer will "
//                                        "be halted in 1 second.",
//                                        "Your battery level is critical, the computer will "
//                                        "be halted in %1 seconds.",
//                                        PowerDevilSettings::waitBeforeSuspendingTime()),
//                                  SLOT(shutdown()), "dialog-warning");
//             } else {
//                 shutdown();
//             }
//             break;
//         case S2Disk:
//             if (PowerDevilSettings::waitBeforeSuspending()) {
//                 emitNotification("criticalbattery",
//                                  i18np("Your battery level is critical, the computer will "
//                                        "be suspended to disk in 1 second.",
//                                        "Your battery level is critical, the computer will "
//                                        "be suspended to disk in %1 seconds.",
//                                        PowerDevilSettings::waitBeforeSuspendingTime()),
//                                  SLOT(suspendToDisk()), "dialog-warning");
//             } else {
//                 suspendToDisk();
//             }
//             break;
//         case S2Ram:
//             if (PowerDevilSettings::waitBeforeSuspending()) {
//                 emitNotification("criticalbattery",
//                                  i18np("Your battery level is critical, the computer "
//                                        "will be suspended to RAM in 1 second.",
//                                        "Your battery level is critical, the computer "
//                                        "will be suspended to RAM in %1 seconds.",
//                                        PowerDevilSettings::waitBeforeSuspendingTime()),
//                                  SLOT(suspendToRam()), "dialog-warning");
//             } else {
//                 suspendToRam();
//             }
//             break;
//         case Standby:
//             if (PowerDevilSettings::waitBeforeSuspending()) {
//                 emitNotification("criticalbattery",
//                                  i18np("Your battery level is critical, the computer "
//                                        "will be put into standby in 1 second.",
//                                        "Your battery level is critical, the computer "
//                                        "will be put into standby in %1 seconds.",
//                                        PowerDevilSettings::waitBeforeSuspendingTime()),
//                                  SLOT(standby()), "dialog-warning");
//             } else {
//                 standby();
//             }
//             break;
//         default:
//             emitNotification("criticalbattery", i18n("Your battery level is critical: "
//                                                      "save your work as soon as possible."),
//                              0, "dialog-warning");
//             break;
//         }
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

void Core::onBatteryRemainingTimeChanged(int time)
{
    emit batteryRemainingTimeChanged(time);
}

void Core::onBrightnessChanged(float brightness)
{
    emit brightnessChanged(brightness);
}

void Core::onButtonPressed(PowerDevil::BackendInterface::ButtonType type)
{
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
    QVariantMap args;
    args["Type"] = "SuspendHybrid";
    ActionPool::instance()->loadAction("SuspendSession", KConfigGroup(), this)->trigger(args);
}

void Core::suspendToDisk()
{
    QVariantMap args;
    args["Type"] = "ToDisk";
    ActionPool::instance()->loadAction("SuspendSession", KConfigGroup(), this)->trigger(args);
}

void Core::suspendToRam()
{
    QVariantMap args;
    args["Type"] = "Suspend";
    ActionPool::instance()->loadAction("SuspendSession", KConfigGroup(), this)->trigger(args);
}

int Core::batteryRemainingTime() const
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

void Core::setBrightness(int percent)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<float>((float)percent);
    ActionPool::instance()->loadAction("BrightnessControl", KConfigGroup(), this)->trigger(args);
}

}

#include "powerdevilcore.moc"
