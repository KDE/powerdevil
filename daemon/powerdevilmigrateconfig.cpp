/*
 *  SPDX-FileCopyrightText: Copyright 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilmigrateconfig.h"

#include "powerdevilenums.h"

#include <PowerDevilActivitySettings.h>
#include <PowerDevilProfileSettings.h>
#include <powerdevil_version.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <functional> // std::invoke
#include <type_traits> // std::remove_cvref_t

namespace
{

template<typename T>
struct IdentityTransformer {
    std::remove_cvref_t<T> operator()(T t)
    {
        return std::move(t);
    }
};

} // anonymous namespace

namespace PowerDevil
{

void migrateActivitiesConfig(KSharedConfig::Ptr profilesConfig)
{
    KConfigGroup migrationGroup = profilesConfig->group(QStringLiteral("Migration"));
    if (migrationGroup.hasKey("MigratedActivitiesToPlasma6")) {
        return;
    }

    // Activity special behavior config written via ActivitySettings, reading must be done manually.
    const KConfigGroup oldActivitiesGroup = profilesConfig->group(QStringLiteral("Activities"));
    if (!oldActivitiesGroup.exists()) {
        return;
    }

    // Every activity has its own group identified by its UUID.
    for (const QString &activityId : oldActivitiesGroup.groupList()) {
        const KConfigGroup oldConfig = oldActivitiesGroup.group(activityId);
        PowerDevil::ActivitySettings newConfig(activityId);

        if (oldConfig.readEntry("mode", "None") == "SpecialBehavior") {
            if (const KConfigGroup oldSB = oldConfig.group(QStringLiteral("SpecialBehavior")); oldSB.exists()) {
                newConfig.setInhibitScreenManagement(oldSB.readEntry("noScreenManagement", false));
                newConfig.setInhibitSuspend(oldSB.readEntry("noSuspend", false));
            }
        }
        newConfig.save();
    }

    migrationGroup.writeEntry("MigratedActivitiesToPlasma6", "powerdevilrc");
    profilesConfig->sync();
}

void migrateProfilesConfig(KSharedConfig::Ptr profilesConfig, bool isMobile, bool isVM, bool canSuspend)
{
    KConfigGroup migrationGroup = profilesConfig->group(QStringLiteral("Migration"));
    if (migrationGroup.hasKey(QStringLiteral("MigratedProfilesToPlasma6"))) {
        return;
    }

    for (QString profileName : {"AC", "Battery", "LowBattery"}) {
        const KConfigGroup oldProfileGroup = profilesConfig->group(profileName);
        if (!oldProfileGroup.exists()) {
            continue;
        }
        PowerDevil::ProfileSettings profileSettings(profileName, isMobile, isVM, canSuspend);

        auto migrateEntry = [&]<typename T, typename Transformer = IdentityTransformer<T>>(KConfigGroup & oldGroup,
                                                                                           const QString &oldKey,
                                                                                           void (ProfileSettings::*setter)(T),
                                                                                           Transformer transform = IdentityTransformer<T>())
        {
            if (!oldGroup.hasKey(oldKey)) {
                return;
            }
            // We know oldKey exists, so use any T value as default because we don't care what's in it
            T newValue = std::invoke(transform, oldGroup.readEntry(oldKey, std::remove_cvref_t<T>{}));
            (profileSettings.*setter)(newValue);
        };

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("KeyboardBrightnessControl")); group.exists()) {
            profileSettings.setUseProfileSpecificKeyboardBrightness(true);
            migrateEntry(group, "value", &ProfileSettings::setKeyboardBrightness);
        } else {
            profileSettings.setUseProfileSpecificKeyboardBrightness(false);
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("BrightnessControl")); group.exists()) {
            profileSettings.setUseProfileSpecificDisplayBrightness(true);
            migrateEntry(group, "value", &ProfileSettings::setDisplayBrightness);
        } else {
            profileSettings.setUseProfileSpecificDisplayBrightness(false);
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("DimDisplay")); group.exists()) {
            profileSettings.setDimDisplayWhenIdle(true);
            migrateEntry(group, "idleTime", &ProfileSettings::setDimDisplayIdleTimeoutSec, [](int oldMsec) {
                return oldMsec / 1000; // standarize on using seconds, see powerdevil issue #3 on KDE Invent
            });
        } else {
            profileSettings.setDimDisplayWhenIdle(false);
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("DPMSControl")); group.exists()) {
            profileSettings.setTurnOffDisplayWhenIdle(true);
            // The "DPMSControl" group used seconds for "idleTime". Unlike other groups which
            // used milliseconds in Plasma 5, this one doesn't need division by 1000.
            migrateEntry(group, "idleTime", &ProfileSettings::setTurnOffDisplayIdleTimeoutSec);
            migrateEntry(group, "idleTimeoutWhenLocked", &ProfileSettings::setTurnOffDisplayIdleTimeoutWhenLockedSec);
            migrateEntry(group, "lockBeforeTurnOff", &ProfileSettings::setLockBeforeTurnOffDisplay);
        } else {
            profileSettings.setTurnOffDisplayWhenIdle(false);
        }

        bool suspendThenHibernate = false;
        bool hybridSuspend = false;

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("SuspendSession")); group.exists()) {
            if (group.readEntry("suspendThenHibernate", false)) {
                suspendThenHibernate = true;
            }
            migrateEntry(group, "suspendType", &ProfileSettings::setAutoSuspendAction, [&](uint oldAction) {
                if (oldAction == 4 /* the old PowerButtonAction::SuspendHybrid */) {
                    hybridSuspend = true;
                    return qToUnderlying(PowerButtonAction::Sleep);
                }
                return oldAction;
            });
            migrateEntry(group, "idleTime", &ProfileSettings::setAutoSuspendIdleTimeoutSec, [](int oldMsec) {
                return oldMsec / 1000; // standarize on using seconds, see powerdevil issue #3 on KDE Invent
            });
            // Note: setSleepMode() is called at the end, completing this section.
        } else {
            profileSettings.setAutoSuspendAction(qToUnderlying(PowerButtonAction::NoAction));
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("HandleButtonEvents")); group.exists()) {
            migrateEntry(group, "powerButtonAction", &ProfileSettings::setPowerButtonAction, [&](uint oldAction) {
                if (oldAction == 4 /* the old PowerButtonAction::SuspendHybrid */) {
                    hybridSuspend = true;
                    return qToUnderlying(PowerButtonAction::Sleep);
                }
                return oldAction;
            });
            migrateEntry(group, "powerDownAction", &ProfileSettings::setPowerDownAction, [&](uint oldAction) {
                if (oldAction == 4 /* the old PowerButtonAction::SuspendHybrid */) {
                    hybridSuspend = true;
                    return qToUnderlying(PowerButtonAction::Sleep);
                }
                return oldAction;
            });
            migrateEntry(group, "lidAction", &ProfileSettings::setLidAction, [&](uint oldAction) {
                if (oldAction == 4 /* the old PowerButtonAction::SuspendHybrid */) {
                    hybridSuspend = true;
                    return qToUnderlying(PowerButtonAction::Sleep);
                }
                return oldAction;
            });
            migrateEntry(group,
                         "triggerLidActionWhenExternalMonitorPresent",
                         &ProfileSettings::setInhibitLidActionWhenExternalMonitorPresent,
                         [](bool shouldTrigger) {
                             return !shouldTrigger;
                         });
        } else {
            profileSettings.setPowerButtonAction(qToUnderlying(PowerButtonAction::NoAction));
            profileSettings.setPowerDownAction(qToUnderlying(PowerButtonAction::NoAction));
            profileSettings.setLidAction(qToUnderlying(PowerButtonAction::NoAction));
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("PowerProfile")); group.exists()) {
            migrateEntry(group, "profile", &ProfileSettings::setPowerProfile);
        }

        if (KConfigGroup group = oldProfileGroup.group(QStringLiteral("RunScript")); group.exists()) {
            switch (group.readEntry("scriptPhase", 0)) {
            case 0: // on profile load
                migrateEntry(group, "scriptCommand", &ProfileSettings::setProfileLoadCommand);
                break;
            case 1: // on profile unload
                migrateEntry(group, "scriptCommand", &ProfileSettings::setProfileUnloadCommand);
                break;
            case 2: // on idle timeout
                migrateEntry(group, "scriptCommand", &ProfileSettings::setIdleTimeoutCommand);
                break;
            }
            migrateEntry(group, "idleTime", &ProfileSettings::setRunScriptIdleTimeoutSec, [](int oldMsec) {
                return oldMsec / 1000; // standarize on using seconds, see powerdevil issue #3 on KDE Invent
            });
        }

        if (suspendThenHibernate) {
            profileSettings.setSleepMode(qToUnderlying(SleepMode::SuspendThenHibernate));
        } else if (hybridSuspend) {
            profileSettings.setSleepMode(qToUnderlying(SleepMode::HybridSuspend));
        }

        profileSettings.save();
    }

    migrationGroup.writeEntry("MigratedProfilesToPlasma6", "powerdevilrc");
    profilesConfig->sync();
}

void migrateConfig(bool isMobile, bool isVM, bool canSuspend)
{
    KSharedConfig::Ptr profilesConfig = KSharedConfig::openConfig(QStringLiteral("powermanagementprofilesrc"));
    // TODO KF7 (or perhaps earlier): Remove Plasma 5->6 migration code and delete powermanagementprofilesrc

    migrateActivitiesConfig(profilesConfig);
    migrateProfilesConfig(profilesConfig, isMobile, isVM, canSuspend);

#if POWERDEVIL_VERSION < QT_VERSION_CHECK(6, 0, 0)
    KSharedConfig::Ptr globalConfig = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    KConfigGroup batteryManagementGroup = globalConfig->group(QStringLiteral("BatteryManagement"));
    if (batteryManagementGroup.readEntry("BatteryCriticalAction", 0) == 4 /* the old PowerButtonAction::SuspendHybrid */) {
        batteryManagementGroup.writeEntry("BatteryCriticalAction", qToUnderlying(PowerButtonAction::Hibernate));
        globalConfig->sync();
    }
#endif
}

} // namespace PowerDevil
