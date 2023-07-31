/*
 *  SPDX-FileCopyrightText: Copyright 2021 Tomaz Canabrava <tcanabrava@kde.org>
 *  SPDX-FileCopyrightText: Copyright 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

#include "powerdevilcore_export.h"

namespace PowerDevil
{

class POWERDEVILCORE_EXPORT ProfileDefaults
{
public:
    static bool defaultUseProfileSpecificDisplayBrightness(const QString &profileGroup);
    static int defaultDisplayBrightness(const QString &profileGroup);

    static bool defaultDimDisplayWhenIdle();
    static int defaultDimDisplayIdleTimeoutSec(const QString &profileGroup, bool isMobile);

    static bool defaultTurnOffDisplayWhenIdle();
    static int defaultTurnOffDisplayIdleTimeoutSec(const QString &profileGroup, bool isMobile);
    static bool defaultLockBeforeTurnOffDisplay(bool isMobile);

    static bool defaultAutoSuspendWhenIdle(bool canSuspendToRam);
    static int defaultAutoSuspendIdleTimeoutSec(const QString &profileGroup, bool isMobile);
    static unsigned int defaultAutoSuspendType();

    static unsigned int defaultPowerButtonAction(bool isMobile);
    static unsigned int defaultPowerDownAction();
    static unsigned int defaultLidAction(bool canSuspendToRam);
};

}
