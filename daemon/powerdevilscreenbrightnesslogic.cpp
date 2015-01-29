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

int ScreenBrightnessLogic::toggled() const
{
    // ignore, we won't toggle the display off
    return -1;
}

int ScreenBrightnessLogic::calculateSteps(int maxValue) const
{
    // We assume that a generally good number of steps for screen brightness is about 10.

    if (maxValue <= 15 || maxValue == 17) {
        // Too few steps, return the number of actual steps.
        return maxValue;
    }

    if (maxValue % 5 == 0 || maxValue >= 80) {
        /* If there are more than 100 actual steps, any value will be displayed nicely.
           If maxValue >= 80 and maxValue % 2 = 0, then 10%, 20%, 30%, 40%, 50%, 60%, 70%, 80% and 90% are displayed nicely.
           If maxValue >= 80 and maxValue % 2 = 1, then it is showed as
            0% 10% 20% 30% 40% 51% 60% 70% 80% 90% 100% or 0% 11% 20% 31% 40% 51% 60% 71% 80% 91% 100%, which looks good.
           If maxValue >= 35 and maxValue % 5 = 0, then it is showed as
            0% 11% 20% 31% 40% 51% 60% 71% 80% 91% 100% (when maxValue % 10 != 0), which also looks good.
           For maxValue == 25 steps are showed as
            0% 12% 20% 32% 40% 52% 60% 72% 80% 92% 100%, which is also fine.
        */
        return 10;
    }

    for (int steps = 10; steps <= 15; steps++) {
        if (maxValue % steps == 0) {
            return steps;
        }
    }

    for (int steps = 9; steps >= 8; steps--) {
        if (maxValue % steps == 0) {
            return steps;
        }
    }

    // 28 different maxValue values between 17 and 79 are left at this point.
    //qCDebug(POWERDEVIL) << "maxValue" << maxValue;

    // Give up and return 10, there is nothing much we can do here.
    return 10;
}

};