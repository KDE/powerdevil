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

#include "powerdevilprofilegenerator.h"

#include "powerdevilenums.h"
#include "powerdevilprofiledefaults.h"

#include <PowerDevilSettings.h>

#include <Solid/Battery>
#include <Solid/Device>

#include <KConfigGroup>
#include <KSharedConfig>

namespace PowerDevil
{
void ProfileGenerator::generateProfiles(bool mobile, bool toRam, bool toDisk)
{
    // Change critical action if default (hibernate) is unavailable
    if (!toDisk) {
        if (!toRam) {
            PowerDevilSettings::setBatteryCriticalAction(PowerDevil::to_underlying(PowerButtonAction::NoAction));
        } else {
            PowerDevilSettings::setBatteryCriticalAction(PowerDevil::to_underlying(PowerButtonAction::SuspendToRam));
        }

        PowerDevilSettings::self()->save();
    }

    // Ok, let's get our config file.
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig);

    // And clear it
    const QStringList groupList = profilesConfig->groupList();
    for (const QString &group : groupList) {
        // Don't delete activity-specific settings
        if (group != "Activities") {
            profilesConfig->deleteGroup(group);
        }
    }

    auto initProfile = [toRam, mobile](KConfigGroup &profile) {
        if (ProfileDefaults::defaultUseProfileSpecificDisplayBrightness(profile.name())) {
            KConfigGroup brightnessControl(&profile, "BrightnessControl");
            brightnessControl.writeEntry("value", ProfileDefaults::defaultDisplayBrightness(profile.name()));
        }

        KConfigGroup handleButtonEvents(&profile, "HandleButtonEvents");
        handleButtonEvents.writeEntry("powerButtonAction", ProfileDefaults::defaultPowerButtonAction(mobile));
        handleButtonEvents.writeEntry("powerDownAction", ProfileDefaults::defaultPowerDownAction());
        handleButtonEvents.writeEntry("lidAction", ProfileDefaults::defaultLidAction(toRam));

        if (ProfileDefaults::defaultDimDisplayWhenIdle()) {
            KConfigGroup dimDisplay(&profile, "DimDisplay");
            dimDisplay.writeEntry("idleTime", ProfileDefaults::defaultDimDisplayIdleTimeoutSec(profile.name(), mobile) * 1000); // milliseconds
        }

        if (ProfileDefaults::defaultTurnOffDisplayWhenIdle()) {
            KConfigGroup dpmsControl(&profile, "DPMSControl");
            dpmsControl.writeEntry("idleTime", ProfileDefaults::defaultTurnOffDisplayIdleTimeoutSec(profile.name(), mobile)); // seconds
            dpmsControl.writeEntry<int>("lockBeforeTurnOff", ProfileDefaults::defaultLockBeforeTurnOffDisplay(mobile));
        }

        if (ProfileDefaults::defaultAutoSuspendWhenIdle(toRam)) {
            KConfigGroup suspendSession(&profile, "SuspendSession");
            suspendSession.writeEntry("idleTime", ProfileDefaults::defaultAutoSuspendIdleTimeoutSec(profile.name(), mobile) * 1000); // milliseconds
            suspendSession.writeEntry("suspendType", ProfileDefaults::defaultAutoSuspendType());
        }
    };

    // Let's start: AC profile before anything else
    KConfigGroup acProfile(profilesConfig, "AC");
    initProfile(acProfile);

    // Powersave
    KConfigGroup batteryProfile(profilesConfig, "Battery");
    initProfile(batteryProfile);

    // Ok, now for aggressive powersave
    KConfigGroup lowBatteryProfile(profilesConfig, "LowBattery");
    initProfile(lowBatteryProfile);

    // Save and be happy
    profilesConfig->sync();
}

}
