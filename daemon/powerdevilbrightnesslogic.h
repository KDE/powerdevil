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


#ifndef POWERDEVIL_BRIGHNTESSLOGIC_H
#define POWERDEVIL_BRIGHNTESSLOGIC_H

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
     * This struct contains information about current brightness state for a single device
     */
    struct BrightnessInfo {
        /** The raw brightness value, from 0 to valueMax */
        int value;
        /** The maximum possible brightness value for this device */
        int valueMax;
        /** The maximum possible brightness step for this device */
        int steps;
    };

    /**
     * Sets the current brightness value.
     *
     * @param value Raw brightness value
     */
    void setValue(int value);

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
    int action(BrightnessKeyType type) const;

    /**
     * Calculates the brightness value of the closest step upwards.
     * (Closest step that is higher than current brightness value).
     *
     * @return The brightness value of the closest step upwards
     */
    virtual int increased() const;

    /**
     * Calculates the brightness value of the closest step downwards.
     * (Closest step that is lower than current brightness value).
     *
     * @return The brightness value of the closest step downwards
     */
    virtual int decreased() const;

    /**
     * Calculates the brightness value of the toggled state.
     * (Sets the brightness value to either 0 or valueMax).
     *
     * @return The brightness value that should be set, or -1 if nothing should be done
     */
    virtual int toggled() const;

    /**
     * Retrieve the current brightness value.
     *
     * @return Raw brightness value, from 0 to valueMax
     */
    int value() const;

    /**
     * Retrieve the maximum possible brightness value for this device.
     *
     * @return Maximum possible brightness value
     */
    int valueMax() const;

    /**
     * Retrieve the brightness step that is closest to the current brightness value.
     *
     * @return Nearest brightness step
     */
    int step() const;

    /**
     * Retrieve the maximum possible brightness step for this instance
     *
     * @return Maximum possible brightness step
     */
    int steps() const;

    /**
     * Retrieve the supplied brightness value expressed as a percentage from 0 to 100
     *
     * @param value Brightness value, from 0 to valueMax
     * @return The brightness percentage for the supplied value
     */
    float percentage(int value) const;

    /**
     * Retrieve a copy of the current brightness state
     *
     * @return A struct that contains the current brightness state.
     */
    const BrightnessInfo info() const;

    /**
     * Convert brightness step to raw brightness value
     *
     * @param step Brightness step, from 0 to steps
     * @return Brightness value that corresponds to the given step
     */
    int stepToValue(int step) const;

    /**
     * Convert raw brightness value to brightness step
     *
     * @param value Brightness value, from 0 to valueMax
     * @return Brightness step that is nearest to the given brightness value
     */
    int valueToStep(int value) const;

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
     * @return the optimal maximum step number
     */
    virtual int calculateSteps(int valueMax) const = 0;

private:
    int m_value = -1;
    int m_valueMax = -1;
    int m_steps = -1;
};

}

#endif // POWERDEVIL_BRIGHNTESSLOGIC_H
