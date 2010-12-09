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
        KSharedConfigPtr oldProfilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);
        if (!oldProfilesConfig->groupList().isEmpty()) {
            // We can upgrade, let's do that.
            upgradeProfiles();
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
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig("powerdevil2profilesrc", KConfig::SimpleConfig);

    // And clear it
    foreach (const QString &group, profilesConfig->groupList()) {
        profilesConfig->deleteGroup(group);
    }

    // Let's start: performance profile before anything else
    KConfigGroup performance(profilesConfig, "Performance");
    performance.writeEntry("icon", "preferences-system-performance");

    // We want to dim the screen after a while, definitely
    {
        KConfigGroup dimDisplay(&performance, "DimDisplay");
        dimDisplay.writeEntry< int >("idleTime", 1800000);
    }
    // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
    {
        KConfigGroup handleButtonEvents(&performance, "HandleButtonEvents");
        handleButtonEvents.writeEntry< uint >("powerButtonAction", 5);
        if (methods.contains(Solid::PowerManagement::SuspendState)) {
            handleButtonEvents.writeEntry< uint >("sleepButtonAction", 1);
            handleButtonEvents.writeEntry< uint >("lidAction", 1);
        } else {
            handleButtonEvents.writeEntry< uint >("sleepButtonAction", 0);
            handleButtonEvents.writeEntry< uint >("lidAction", 6);
        }
    }
    // And we also want to turn off the screen after another long while
    {
        KConfigGroup dpmsControl(&performance, "DPMSControl");
        dpmsControl.writeEntry< uint >("idleTime", 3600000);
    }

    // Assign the profile, of course!
    PowerDevilSettings::setACProfile(performance.name());

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
        // Ok, we need a pair more profiles: powersave and aggressive powersave.
        // Also, now we want to handle brightness in performance.
        {
            KConfigGroup brightnessControl(&performance, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", 90);
        }

        // Powersave
        KConfigGroup powersave(profilesConfig, "Powersave");
        powersave.writeEntry("icon", "preferences-system-power-management");
        // Less brightness.
        {
            KConfigGroup brightnessControl(&powersave, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", 60);
        }
        // We want to dim the screen after a while, definitely
        {
            KConfigGroup dimDisplay(&powersave, "DimDisplay");
            dimDisplay.writeEntry< int >("idleTime", 300000);
        }
        // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
        {
            KConfigGroup handleButtonEvents(&powersave, "HandleButtonEvents");
            handleButtonEvents.writeEntry< uint >("powerButtonAction", 5);
            if (methods.contains(Solid::PowerManagement::SuspendState)) {
                handleButtonEvents.writeEntry< uint >("sleepButtonAction", 1);
                handleButtonEvents.writeEntry< uint >("lidAction", 1);
            } else {
                handleButtonEvents.writeEntry< uint >("sleepButtonAction", 0);
                handleButtonEvents.writeEntry< uint >("lidAction", 6);
            }
        }
        // We want to turn off the screen after another while
        {
            KConfigGroup dpmsControl(&powersave, "DPMSControl");
            dpmsControl.writeEntry< uint >("idleTime", 600000);
        }
        // Last but not least, we want to suspend after a rather long period of inactivity
        if (methods.contains(Solid::PowerManagement::SuspendState) || methods.contains(Solid::PowerManagement::HibernateState)) {
            KConfigGroup suspendSession(&powersave, "SuspendSession");
            suspendSession.writeEntry< uint >("idleTime", 900000);
            if (!methods.contains(Solid::PowerManagement::SuspendState)) {
                suspendSession.writeEntry< uint >("suspendType", 2);
            } else {
                suspendSession.writeEntry< uint >("suspendType", 1);
            }
        }


        // Ok, now for aggressive powersave
        KConfigGroup aggrPowersave(profilesConfig, "Aggressive powersave");
        aggrPowersave.writeEntry("icon", "battery-low");
        // Less brightness.
        {
            KConfigGroup brightnessControl(&aggrPowersave, "BrightnessControl");
            brightnessControl.writeEntry< int >("value", 30);
        }
        // We want to dim the screen after a while, definitely
        {
            KConfigGroup dimDisplay(&aggrPowersave, "DimDisplay");
            dimDisplay.writeEntry< int >("idleTime", 120000);
        }
        // Show the dialog when power button is pressed and suspend on suspend button pressed and lid closed (if supported)
        {
            KConfigGroup handleButtonEvents(&aggrPowersave, "HandleButtonEvents");
            handleButtonEvents.writeEntry< uint >("powerButtonAction", 5);
            if (methods.contains(Solid::PowerManagement::SuspendState)) {
                handleButtonEvents.writeEntry< uint >("sleepButtonAction", 1);
                handleButtonEvents.writeEntry< uint >("lidAction", 1);
            } else {
                handleButtonEvents.writeEntry< uint >("sleepButtonAction", 0);
                handleButtonEvents.writeEntry< uint >("lidAction", 6);
            }
        }
        // We want to turn off the screen after another while
        {
            KConfigGroup dpmsControl(&aggrPowersave, "DPMSControl");
            dpmsControl.writeEntry< uint >("idleTime", 300000);
        }
        // Last but not least, we want to suspend after a rather long period of inactivity
        if (methods.contains(Solid::PowerManagement::SuspendState) || methods.contains(Solid::PowerManagement::HibernateState)) {
            KConfigGroup suspendSession(&aggrPowersave, "SuspendSession");
            suspendSession.writeEntry< uint >("idleTime", 600000);
            if (!methods.contains(Solid::PowerManagement::SuspendState)) {
                suspendSession.writeEntry< uint >("suspendType", 2);
            } else {
                suspendSession.writeEntry< uint >("suspendType", 1);
            }
        }

        // Assign profiles
        PowerDevilSettings::setBatteryProfile(powersave.name());
        PowerDevilSettings::setLowProfile(aggrPowersave.name());
        PowerDevilSettings::setWarningProfile(aggrPowersave.name());
    }

    // Save and be happy
    PowerDevilSettings::self()->writeConfig();
    profilesConfig->sync();

    return ResultGenerated;
}

void ProfileGenerator::upgradeProfiles()
{
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
            dpmsControl.writeEntry< uint >("idleTime", oldGroup.readEntry< int >("DPMSPowerOff", 30) * 60 * 1000);
        }
        // Script
        if (!oldGroup.readEntry< QString >("scriptpath", QString()).isEmpty()) {
            KConfigGroup runScript(&newGroup, "RunScript");
            runScript.writeEntry< QString >("scriptCommand", oldGroup.readEntry< QString >("scriptpath", QString()));
            runScript.writeEntry< uint >("scriptPhase", 0);
        }
        // SuspendSession
        if (oldGroup.readEntry< int >("idleAction", 0) > 0) {
            KConfigGroup suspendSession(&newGroup, "SuspendSession");
            suspendSession.writeEntry< uint >("idleTime", oldGroup.readEntry< int >("idleTime", 30) * 60 * 1000);
            if (!methods.contains(Solid::PowerManagement::SuspendState)) {
                suspendSession.writeEntry< uint >("suspendType", 2);
            } else {
                suspendSession.writeEntry< uint >("suspendType", 1);
            }
        }
        // Buttons
        if (oldGroup.readEntry< int >("powerButtonAction", 0) > 0 || oldGroup.readEntry< int >("lidAction", 0) > 0) {
            KConfigGroup suspendSession(&newGroup, "SuspendSession");
            suspendSession.writeEntry< uint >("idleTime", oldGroup.readEntry< int >("idleTime", 30) * 60 * 1000);
            if (!methods.contains(Solid::PowerManagement::SuspendState)) {
                suspendSession.writeEntry< uint >("suspendType", 2);
            } else {
                suspendSession.writeEntry< uint >("suspendType", 1);
            }
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

}
