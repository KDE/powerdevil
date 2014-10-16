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

#include "powerdevilkeyboardbrightnesslogic.h"
#include "powerdevil_debug.h"
#include <QDebug>

namespace PowerDevil
{

int KeyboardBrightnessLogic::calculateStepMax(int maxValue) const
{
    // We assume that a generally good number of steps for keyboard brightness is about 5.

    if (maxValue <= 7) {
        // Too few steps, return the number of actual steps.
       return maxValue;
    }

    if (maxValue % 5 == 0 || maxValue >= 80) {
        // When there are more than 80 actual steps, 20%, 40%, 60%, and 80% are always displayed nicely.
        return 5;
    }

    if (maxValue % 4 == 0)
        return 4;

    if (maxValue % 6 == 0)
        return 6;

    if (maxValue % 3 == 0)
        return 3;

    // 29 different maxValue values between 11 and 79 are left at this point.
    //qCDebug(POWERDEVIL) << "maxValue" << maxValue;

    // Give up and return 5, there is nothing much we can do here.
    return 5;
}

};