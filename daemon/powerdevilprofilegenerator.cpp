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

#include <QtCore/QFile>

#include <Solid/Device>
#include <Solid/Battery>
#include <Solid/PowerManagement>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KNotification>
#include <KIcon>
#include <KStandardDirs>

namespace PowerDevil {

ProfileGenerator::GeneratorResult ProfileGenerator::generateProfiles(bool tryUpgrade)
{
    if (tryUpgrade) {
        bool isUpgraded = false;
        KSharedConfigPtr oldProfilesConfigv1 = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);
        if (!oldProfilesConfigv1->groupList().isEmpty()) {
            // We can upgrade from v1, let's do that.
            upgradeProfilesv1();
            isUpgraded = true;
        }
        KSharedConfigPtr oldProfilesConfigv2 = KSharedConfig::openConfig("powerdevil2profilesrc", KConfig::SimpleConfig);
        if (!oldProfilesConfigv2->groupList().isEmpty()) {
            // We can upgrade from v2, let's do that.
            upgradeProfilesv2();
            isUpgraded = true;
        }

        if (isUpgraded) {
            return ResultUpgraded;
        }
    }
    QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

    // Let's change some defaults
    if (!methods.contains(Solid::PowerManagement::SuspendState)) {
        if (!methods.contains(Solid::PowerManagement::HibernateState)) {
            PowerDevilSettings::setBatteryCriticalAction(0);
        } else {
            PowerDevilSettings::setBatteryCriticalAction(2);
        }
    }

    // Ok, let's get our config file.
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig);

    // And clear it
    foreach (const QString &group, profilesConfig->groupList()) {
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
        KConfigGroup dimDisplay(&acProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 300000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&acProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (methods.contains(Solid::PowerManagement::SuspendState)) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }
    // And we also want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&acProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 600);
    }

    // Easy part done. Now, any batteries?
    int batteryCount = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Solid::Device dev = device;
        Solid::Battery *b = qobject_cast<Solid::Battery*> (dev.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->type() != Solid::Battery::PrimaryBattery && b->type() != Solid::Battery::UpsBattery) {
            continue;
        }
        ++batteryCount;
    }

    if (batteryCount > 0) {
        // Then we want to handle brightness in performance.
        {
            KConfigGroup brightnessControl(&acProfile, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", 100);
        }
    }

    // Powersave
    KConfigGroup batteryProfile(profilesConfig, "Battery");
    batteryProfile.writeEntry("icon", "battery-060");
    // Less brightness.
    {
        KConfigGroup brightnessControl(&batteryProfile, "BrightnessControl");
        brightnessControl.writeEntry< int >("value", 60);
    }
    // We want to dim the screen after a while, definitely
    {
        KConfigGroup dimDisplay(&batteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 120000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&batteryProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (methods.contains(Solid::PowerManagement::SuspendState)) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }
    // We want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&batteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 300);
    }
    // Last but not least, we want to suspend after a rather long period of inactivity
    if (methods.contains(Solid::PowerManagement::SuspendState)) {
        KConfigGroup suspendSession(&batteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", 600000);
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
        KConfigGroup dimDisplay(&lowBatteryProfile, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 60000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&lowBatteryProfile, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", LogoutDialogMode);
        if (methods.contains(Solid::PowerManagement::SuspendState)) {
            handleButtonEvents.writeEntry< uint >("lidAction", ToRamMode);
        } else {
            handleButtonEvents.writeEntry< uint >("lidAction", TurnOffScreenMode);
        }
    }
    // We want to turn off the screen after another while
    {
        KConfigGroup dpmsControl(&lowBatteryProfile, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 120);
    }
    // Last but not least, we want to suspend after a rather long period of inactivity
    if (methods.contains(Solid::PowerManagement::SuspendState)) {
        KConfigGroup suspendSession(&lowBatteryProfile, "SuspendSession");
        suspendSession.writeEntry< uint >("idleTime", 300000);
        suspendSession.writeEntry< uint >("suspendType", ToRamMode);
    }

    // Save and be happy
    profilesConfig->sync();

    return ResultGenerated;
}

void ProfileGenerator::upgradeProfilesv1()
{
    QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

    // Let's change some defaults
    if (!methods.contains(Solid::PowerManagement::SuspendState)) {
        if (!methods.contains(Solid::PowerManagement::HibernateState)) {
            PowerDevilSettings::setBatteryCriticalAction(None);
        } else {
            PowerDevilSettings::setBatteryCriticalAction(ToDiskMode);
        }
    } else {
        PowerDevilSettings::setBatteryCriticalAction(ToRamMode);
    }

    // Ok, let's get our config file.
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powerdevil2profilesrc", KConfig::SimpleConfig);
    KSharedConfigPtr oldProfilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    // And clear it
    foreach (const QString &group, profilesConfig->groupList()) {
        profilesConfig->deleteGroup(group);
    }

    foreach (const QString &group, oldProfilesConfig->groupList()) {
        KConfigGroup oldGroup = oldProfilesConfig->group(group);
        KConfigGroup newGroup(profilesConfig, oldGroup.readEntry< QString >("name", QString()));

        // Read stuff
        // Brightness.
        {
            KConfigGroup brightnessControl(&newGroup, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", oldGroup.readEntry< int >("brightness", 100));
        }
        // Dim screen
        if (oldGroup.readEntry< bool >("dimOnIdle", false)) {
            KConfigGroup dimDisplay(&newGroup, "DimDisplay");
            dimDisplay.writeEntry< int >("idleTime", oldGroup.readEntry< int >("dimOnIdleTime", 30) * 60 * 1000);
        }
        // DPMS
        if (oldGroup.readEntry< bool >("DPMSEnabled", false) && oldGroup.readEntry< int >("DPMSPowerOff", 0) > 0) {
            KConfigGroup dpmsControl(&newGroup, "DPMSControl");
            dpmsControl.writeEntry< uint >("idleTime", oldGroup.readEntry< int >("DPMSPowerOff", 30) * 60);
        }
        // Script
        if (!oldGroup.readEntry< QString >("scriptpath", QString()).isEmpty()) {
            KConfigGroup runScript(&newGroup, "RunScript");
            runScript.writeEntry< QString >("scriptCommand", oldGroup.readEntry< QString >("scriptpath", QString()));
            runScript.writeEntry< uint >("scriptPhase", 0);
        }
        // SuspendSession
        if (oldGroup.readEntry< uint >("idleAction", 0) > 0) {
            KConfigGroup suspendSession(&newGroup, "SuspendSession");
            suspendSession.writeEntry< uint >("idleTime", oldGroup.readEntry< int >("idleTime", 30) * 60 * 1000);
            suspendSession.writeEntry< uint >("suspendType", upgradeOldAction(oldGroup.readEntry< uint >("idleAction", 0)));
        }
        // Buttons
        if (oldGroup.readEntry< uint >("powerButtonAction", 0) > 0 || oldGroup.readEntry< uint >("lidAction", 0) > 0) {
            KConfigGroup handleButtons(&newGroup, "HandleButtonEvents");
            handleButtons.writeEntry< uint >("powerButtonAction", upgradeOldAction(oldGroup.readEntry< uint >("powerButtonAction", 0)));
            handleButtons.writeEntry< uint >("lidAction", upgradeOldAction(oldGroup.readEntry< uint >("lidAction", 0)));
        }
    }

    // Save and be happy
    profilesConfig->sync();

    // We also want to backup and erase the old profiles.
    QString oldProfilesFile = KGlobal::dirs()->findResource("config", "powerdevilprofilesrc");
    if (!oldProfilesFile.isEmpty()) {
        // Backup
        QString bkProfilesFile = oldProfilesFile;
        bkProfilesFile.append(".old");
        KConfig *bkConfig = oldProfilesConfig->copyTo(bkProfilesFile);
        if (bkConfig != 0) {
            bkConfig->sync();
            delete bkConfig;

            // Delete the old profiles now.
            QFile::remove(oldProfilesFile);
        }
    }
}

void ProfileGenerator::upgradeProfilesv2()
{
    // Ok, let's get our config file.
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig);
    KSharedConfigPtr oldProfilesConfig = KSharedConfig::openConfig("powerdevil2profilesrc", KConfig::SimpleConfig);

    // And clear it
    foreach (const QString &group, profilesConfig->groupList()) {
        // Don't delete activity-specific settings
        if (group != "Activities") {
            profilesConfig->deleteGroup(group);
        }
    }

    // Ok: back in the days, which profile we used for which task?
    {
        KConfigGroup oldAC = oldProfilesConfig.data()->group(PowerDevilSettings::aCProfile());
        KConfigGroup newGroup(profilesConfig, "AC");

        oldAC.copyTo(&newGroup);
    }
    {
        KConfigGroup oldBattery = oldProfilesConfig.data()->group(PowerDevilSettings::batteryProfile());
        KConfigGroup newGroup(profilesConfig, "Battery");

        oldBattery.copyTo(&newGroup);
    }
    {
        KConfigGroup oldLowBattery = oldProfilesConfig.data()->group(PowerDevilSettings::lowProfile());
        KConfigGroup newGroup(profilesConfig, "LowBattery");

        oldLowBattery.copyTo(&newGroup);
    }

    // Save and be happy
    profilesConfig->sync();

    // We also want to backup and erase the old profiles.
    QString oldProfilesFile = KGlobal::dirs()->findResource("config", "powerdevil2profilesrc");
    if (!oldProfilesFile.isEmpty()) {
        // Backup
        QString bkProfilesFile = oldProfilesFile;
        bkProfilesFile.append(".old");
        KConfig *bkConfig = oldProfilesConfig->copyTo(bkProfilesFile);
        if (bkConfig != 0) {
            bkConfig->sync();
            delete bkConfig;

            // Delete the old profiles now.
            QFile::remove(oldProfilesFile);
        }
    }
}

uint ProfileGenerator::upgradeOldAction(uint oldAction)
{
    switch ((OldIdleAction)oldAction) {
        case Standby:
        case S2Ram:
            return ToRamMode;
        case S2Disk:
            return ToDiskMode;
        case Shutdown:
            return ShutdownMode;
        case Lock:
            return LockScreenMode;
        case ShutdownDialog:
            return LogoutDialogMode;
        case TurnOffScreen:
            return TurnOffScreenMode;
        default:
            return 0;
    }
}

}
