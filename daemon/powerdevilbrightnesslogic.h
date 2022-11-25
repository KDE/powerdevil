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


#ifndef POWERDEVIL_BRIGHTNESSLOGIC_H
#define POWERDEVIL_BRIGHTNESSLOGIC_H

#include "brightnessvalue.h"

namespace PowerDevil {

class BrightnessLogic
{

public:
    BrightnessLogic();
    virtual ~BrightnessLogic() = default;

    /**
     * This enum defines the different types brightness keys.
     *
     * - Increase: Key to increase brightness (Qt::Key_MonBrightnessUp or Qt::Key_KeyboardBrightnessUp)
     * - Decrease: Key to decrease brightness (Qt::Key_MonBrightnessDown or Qt::Key_KeyboardBrightnessDown)
     * - Toggle: Key to toggle backlight (Qt::Key_KeyboardBacklightOnOff)
     */
    enum BrightnessKeyType { Increase, Decrease, Toggle };

    /**
     * A brightness step value.
     * Not to be confused with a brightness value.
     */
    typedef int Step;

    /**
     * This struct contains information about current brightness state for a single device
     */
    struct BrightnessInfo {
        /** The brightness value, from 0 to valueMax */
        ActualBrightness value;
        /** The maximum possible brightness value for this device */
        int valueMax;
        /** The maximum possible brightness step for this device */
        Step steps;
    };

    /**
     * Sets the current brightness value.
     *
     * @param value Perceived brightness value
     */
    void setValue(PerceivedBrightness value);

    /**
     * Sets the maximum brightness value.
     *
     * @param value Maximum brightness value
     */
    void setValueMax(int valueMax);

    /**
     * Calculate new brightness value that should be set by an action.
     *
     * @param type The action type of the key that was pressed.
     * @return The brightness value that the action should set, or -1 if nothing should be done
     */
    PerceivedBrightness action(BrightnessKeyType type) const;

    /**
     * Calculates the brightness value of the closest step upwards.
     * (Closest step that is higher than current brightness value).
     *
     * @return The brightness value of the closest step upwards
     */
    virtual PerceivedBrightness increased() const;

    /**
     * Calculates the brightness value of the closest step downwards.
     * (Closest step that is lower than current brightness value).
     *
     * @return The brightness value of the closest step downwards
     */
    virtual PerceivedBrightness decreased() const;

    /**
     * Calculates the brightness value of the toggled state.
     * (Sets the brightness value to either 0 or valueMax).
     *
     * @return The brightness value that should be set, or -1 if nothing should be done
     */
    virtual PerceivedBrightness toggled() const;

    /**
     * Retrieve the current brightness value.
     *
     * @return Raw brightness value, from 0 to valueMax
     */
    PerceivedBrightness value() const;

    /**
     * Retrieve the maximum possible brightness value for this device.
     *
     * @return Maximum possible brightness value
     */
    int valueMax() const;

    /**
     * Retrieve the brightness step that is closest to the current brightness value.
     * Assumes that m_value satisfies [0, maxValue] and that m_maxValue and m_numSteps are > 0
     *
     * @return Nearest brightness step [0, max step] uses true rounding
     */
    Step closestStep() const;

    /**
     * Retrieve the maximum possible brightness step for this instance
     *
     * @return Maximum possible brightness step
     */
    Step steps() const;

    /**
     * Retrieve the supplied brightness value expressed as a percentage from 0 to 100
     *
     * @param value Brightness value, from 0 to valueMax
     * @return The brightness percentage for the supplied value
     */
    float percentage(PerceivedBrightness value) const;

    /**
     * Retrieve a copy of the current brightness state
     *
     * @return A struct that contains the current brightness state.
     */
    const BrightnessInfo info() const;

    /**
     * Convert brightness step to raw brightness value.
     *
     * @param step Brightness step, from 0 to steps
     * @return Brightness value that corresponds to the given step.
     * Constrains output to [0, maxBrightness]
     */
    PerceivedBrightness stepToValue(Step step) const;

    /**
     * @param value Brightness value
     * @return Brightness step closest to the given value that safisfies [0, brightness max]
     */
    Step valueToStep(PerceivedBrightness value) const;

protected:

    /**
     * Calculate the optimal number of brightness steps.
     *
     * It should be based on three assumptions:
     * 1) The user generally expects to see equal brightness steps, and likes round percentage numbers.
     * 2) Actual brightness is rounded to whole percents before being displayed to the user.
     * 3) An assumption on a generally good number of brightness steps, which varies with implementations.
     *
     * This function does not depend on anything except the argument.
     *
     * @param valueMax the maximum brightness value for which we want to calculate the number of steps
     * @return The highest brightness Step value.
     */
    virtual Step calculateSteps(PerceivedBrightness valueMax) const = 0;

private:
    PerceivedBrightness m_value = PerceivedBrightness(-1);
    PerceivedBrightness m_valueMax = PerceivedBrightness(-1);
    Step m_maxStep = 0;
};

}

#endif // POWERDEVIL_BRIGHTNESSLOGIC_H
