/***************************************************************************
 *   Copyright (C) 2014 by Nikita Skovoroda <chalkerx@gmail.com>            *
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

#include "powerdevilscreenbrightnesslogic.h"
#include "powerdevil_debug.h"
#include <QDebug>

namespace PowerDevil
{

PerceivedBrightness ScreenBrightnessLogic::toggled() const
{
    // ignore, we won't toggle the display off
    return PerceivedBrightness(-1);
}

BrightnessLogic::Step ScreenBrightnessLogic::calculateSteps(PerceivedBrightness max) const
{
    int maxValue = (int)max;
    // We assume that the preferred number of steps for screen brightness is 20, but we don't want more.

    if (maxValue <= 20) {
        // Too few steps, return the number of actual steps.
        // Those would be uniform, but not round in some cases.
        return maxValue;
    }

    if (maxValue >= 100 || maxValue % 20 == 0 || (maxValue >= 80 && maxValue % 4 == 0)) {
        // In this case all 20 steps are perfect.
        return 20;
    }

    // At this point we have maxValue in the range 21-79 which probably is a rare case.

    if (maxValue >= 34 || maxValue == 32 || maxValue == 28) {
        // In this case all 20 steps are matched +-1%, which is fine.
        return 20;
    }

    // At this point we have maxValue in the range 21-33.
    // Trying to make 20 steps from here will make them not uniform.

    if (maxValue % 5 == 0) {
        // For maxValue == 30 there are 10 even and round steps.
        // For maxValue == 25 steps are shown as
        //  0% 12% 20% 32% 40% 52% 60% 72% 80% 92% 100%, which is also fine.
        return 10;
    }

    // Trying hard to find an uniform steps set
    for (int steps = 9; steps <= 14; steps++) {
        if (maxValue % steps == 0) {
            return steps;
        }
    }

    // 4 different maxValue values left: 21, 23, 29, 31.
    // Those produce +-2% on 10 steps, there is nothing better we can do here.
    return 10;
}

};
