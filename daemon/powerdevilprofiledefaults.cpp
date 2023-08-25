/*
 *  SPDX-FileCopyrightText: Copyright 2021 Tomaz Canabrava <tcanabrava@kde.org>
 *  SPDX-FileCopyrightText: Copyright 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilprofiledefaults.h"

#include "powerdevilenums.h"

namespace PowerDevil
{

bool ProfileDefaults::defaultUseProfileSpecificDisplayBrightness(const QString &profileGroup)
{
    return profileGroup == QStringLiteral("AC")        ? false
        : profileGroup == QStringLiteral("Battery")    ? false
        : profileGroup == QStringLiteral("LowBattery") ? true
                                                       : false; // any other profileGroup
}

int ProfileDefaults::defaultDisplayBrightness(const QString &profileGroup)
{
    return profileGroup == QStringLiteral("AC")        ? 70 // not managed by default, see above
        : profileGroup == QStringLiteral("Battery")    ? 70 // not managed by default, see above
        : profileGroup == QStringLiteral("LowBattery") ? 30 // less brightness
                                                       : 70; // any other profileGroup
}

bool ProfileDefaults::defaultDimDisplayWhenIdle()
{
    // We want to dim the screen after a while, definitely
    return true;
}

int ProfileDefaults::defaultDimDisplayIdleTimeoutSec(const QString &profileGroup, bool isMobile)
{
    if (isMobile) {
        return profileGroup == QStringLiteral("AC")        ? 300 // 5 minutes
            : profileGroup == QStringLiteral("Battery")    ? 30 // half a minute
            : profileGroup == QStringLiteral("LowBattery") ? 30 // half a minute
                                                           : 30; // any other profileGroup
    }

    return profileGroup == QStringLiteral("AC")        ? 300 // 5 minutes
        : profileGroup == QStringLiteral("Battery")    ? 120 // 2 minutes
        : profileGroup == QStringLiteral("LowBattery") ? 60 // 1 minute
                                                       : 300; // any other profileGroup
}

bool ProfileDefaults::defaultTurnOffDisplayWhenIdle()
{
    // Screen always gets turned off by default, the question is just how to long to wait
    return true;
}

int ProfileDefaults::defaultTurnOffDisplayIdleTimeoutSec(const QString &profileGroup, bool isMobile)
{
    if (isMobile) {
        return profileGroup == QStringLiteral("AC")        ? 60 // 1 minute
            : profileGroup == QStringLiteral("Battery")    ? 60 // 1 minute
            : profileGroup == QStringLiteral("LowBattery") ? 30 // half a minute
                                                           : 60; // any other profileGroup
    }

    return profileGroup == QStringLiteral("AC")        ? 600 // 10 minutes
        : profileGroup == QStringLiteral("Battery")    ? 300 // 5 minutes
        : profileGroup == QStringLiteral("LowBattery") ? 120 // 2 minutes
                                                       : 600; // any other profileGroup
}

bool ProfileDefaults::defaultLockBeforeTurnOffDisplay(bool isMobile)
{
    return isMobile;
}

bool ProfileDefaults::defaultAutoSuspendWhenIdle(bool canSuspendToRam)
{
    // Even on AC power, suspend after a rather long period of inactivity. Energy is precious!
    return canSuspendToRam;
}

int ProfileDefaults::defaultAutoSuspendIdleTimeoutSec(const QString &profileGroup, bool isMobile)
{
    if (isMobile) {
        return profileGroup == QStringLiteral("AC")        ? 420 // 7 minutes
            : profileGroup == QStringLiteral("Battery")    ? 300 // 5 minutes
            : profileGroup == QStringLiteral("LowBattery") ? 300 // 5 minutes
                                                           : 300; // any other profileGroup
    }

    return profileGroup == QStringLiteral("AC")        ? 900 // 15 minutes
        : profileGroup == QStringLiteral("Battery")    ? 600 // 10 minutes
        : profileGroup == QStringLiteral("LowBattery") ? 300 // 5 minutes
                                                       : 900; // any other profileGroup
}

unsigned int ProfileDefaults::defaultAutoSuspendType()
{
    return qToUnderlying(PowerButtonAction::SuspendToRam);
}

unsigned int ProfileDefaults::defaultPowerButtonAction(bool isMobile)
{
    return qToUnderlying(isMobile ? PowerButtonAction::ToggleScreenOnOff : PowerButtonAction::PromptLogoutDialog);
}

unsigned int ProfileDefaults::defaultPowerDownAction()
{
    return qToUnderlying(PowerButtonAction::PromptLogoutDialog);
}

unsigned int ProfileDefaults::defaultLidAction(bool canSuspendToRam)
{
    return qToUnderlying(canSuspendToRam ? PowerButtonAction::SuspendToRam : PowerButtonAction::TurnOffScreen);
}

} // namespace PowerDevil
