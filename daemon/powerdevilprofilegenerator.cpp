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

#include <PowerDevilSettings.h>

#include <Solid/Device>
#include <Solid/Battery>

#include <KConfigGroup>
#include <KSharedConfig>

namespace PowerDevil {

void ProfileGenerator::generateProfiles(bool toRam, bool toDisk)
{
    // Change critical action if default (hibernate) is unavailable
    if (!toDisk) {
        if (!toRam) {
            PowerDevilSettings::setBatteryCriticalAction(0);
        } else {
            PowerDevilSettings::setBatteryCriticalAction(1);
        }

        PowerDevilSettings::self()->save();
    }

    const bool mobile = !qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MOBILE");

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

    // Let's start: AC profile before anything else
    KConfigGroup acProfile(profilesConfig, "AC");
    acProfile.writeEntry("icon", "battery-charging");

    // We want to dim the screen after a while, definitely
    {
        const auto timeout = 300000;
        KConfigGroup dimDisplay(&acProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", timeout);

        // and also turn the keyboard backlight off after a while
        KConfigGroup turnOffKeyboardBacklight(&acProfile, "TurnOffKeyboardBacklight");
        turnOffKeyboardBacklight.writeEntry<int>("idleTime", timeout / 2);
    }

    auto initLid = [toRam, mobile](KConfigGroup &profile)
    {
        const Modes defaultPowerButtonAction = mobile ? ToggleScreenOnOffMode : LogoutDialogMode;

        KConfigGroup handleButtonEvents(&profile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", defaultPowerButtonAction);
        handleButtonEvents.writeEntry< uint >("powerDownAction", LogoutDialogMode);
        if (toRam) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    };

    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    initLid(acProfile);

    // And we also want to turn off the screen after another while
    {
        // on mobile, 1 minute, on desktop 10 minutes
        auto timeout = mobile ? 60 : 600;
        KConfigGroup dpmsControl(&acProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", timeout);
        dpmsControl.writeEntry< uint >("lockBeforeTurnOff", mobile);
    }

    // Powersave
    KConfigGroup batteryProfile(profilesConfig, "Battery");
    batteryProfile.writeEntry("icon", "battery-060");
    // We want to dim the screen after a while, definitely
    {
        // on mobile 30 seconds, on desktop 2 minutes
        // config is in the miliseconds
        auto timeout = mobile ? 30000 : 120000;
        KConfigGroup dimDisplay(&batteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", timeout);

        // and also turn the keyboard backlight off after a while
        KConfigGroup turnOffKeyboardBacklight(&batteryProfile, "TurnOffKeyboardBacklight");
        // DimDisplay starts dimming at half the timeout already...
        turnOffKeyboardBacklight.writeEntry<int>("idleTime", timeout / 2);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    initLid(batteryProfile);

    // We want to turn off the screen after another while
    {
        // on mobile, 1 minute, on laptop 5 minutes
        auto timeout = mobile ? 60 : 300;
        KConfigGroup dpmsControl(&batteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", timeout);
        dpmsControl.writeEntry< uint >("lockBeforeTurnOff", mobile);
    }

    // Last but not least, we want to suspend after a rather long period of inactivity
    if (toRam) {
        // on mobile, 5 minute, on laptop 10 minutes
        auto timeout = mobile ? 300000 : 600000;
        KConfigGroup suspendSession(&batteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", timeout);
        suspendSession.writeEntry< uint >("suspendType", ToRamMode);
    }


    // Ok, now for aggressive powersave
    KConfigGroup lowBatteryProfile(profilesConfig, "LowBattery");
    lowBatteryProfile.writeEntry("icon", "battery-low");
    // Less brightness.
    {
        KConfigGroup brightnessControl(&lowBatteryProfile, "BrightnessControl");
        brightnessControl.writeEntry< int >("value", 30);
    }
    // We want to dim the screen after a while, definitely
    {
        // on mobile 30 seconds, on desktop 1 minute
        // config is in the miliseconds
        auto timeout = mobile ? 30000 : 60000;
        KConfigGroup dimDisplay(&lowBatteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", timeout);

        // and also turn the keyboard backlight off after a while
        KConfigGroup turnOffKeyboardBacklight(&lowBatteryProfile, "TurnOffKeyboardBacklight");
        turnOffKeyboardBacklight.writeEntry<int>("idleTime", timeout / 2);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    initLid(lowBatteryProfile);

    // We want to turn off the screen after another while
    {
        // on mobile, half minute, on laptop 2 minutes
        auto timeout = mobile ? 30 : 120;
        KConfigGroup dpmsControl(&lowBatteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", timeout);
        dpmsControl.writeEntry< uint >("lockBeforeTurnOff", mobile);
    }

    // Last but not least, we want to suspend after a rather long period of inactivity
    // on mobile by default never suspend, if device wants to suspend, it will enable
    // using configuration overlay
    if (toRam) {
        // config is in the miliseconds
        KConfigGroup suspendSession(&lowBatteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", 300000);
        suspendSession.writeEntry< uint >("suspendType", ToRamMode);
    }

    // Save and be happy
    profilesConfig->sync();
}

}
