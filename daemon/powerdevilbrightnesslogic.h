/*
 *   SPDX-FileCopyrightText: 2014 Nikita Skovoroda <chalkerx@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

namespace PowerDevil
{
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
     * - IncreaseSmall: Key to increase brightness by 1% (Qt::Key_MonBrightnessUp or Qt::Key_KeyboardBrightnessUp + Qt::ShiftModifier)
     * - DecreaseSmall: Key to decrease brightness by 1% (Qt::Key_MonBrightnessDown or Qt::Key_KeyboardBrightnessDown + Qt::ShiftModifier)
     */
    enum BrightnessKeyType {
        Increase,
        Decrease,
        Toggle,
        IncreaseSmall,
        DecreaseSmall,
    };

    /**
     * This struct contains information about current brightness state for a single device
     */
    struct BrightnessInfo {
        /** The raw brightness value, from 0 to valueMax */
        int value = -1;
        /**
         * The minimum practical brightness value for this device. Usually 0, but should be
         * overridden for device types where a higher minimum value brightness makes sense.
         * For example, for screens where a value of 0 sometimes turns off the backlight
         * completely, which is not wanted there.
         */
        int valueMin = -1;
        /** The maximum possible brightness value for this device */
        int valueMax = -1;
        /** The most recent brightness before toggling off */
        int valueBeforeTogglingOff = -1;
        /** The maximum possible brightness step for this device */
        int steps = -1;
    };

    /**
     * Sets the current brightness value.
     *
     * @param value Raw brightness value
     */
    void setValue(int value);

    /**
     * Sets the minimum and maximum brightness value.
     *
     * @param valueMin Minimum brightness value
     * @param valueMax Maximum brightness value
     */
    void setValueRange(int valueMin, int valueMax);

    /**
     * Sets the last active brightness value.
     *
     * @param value Last active brightness value
     */
    void setValueBeforeTogglingOff(int valueBeforeTogglingOff);

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
    int increased() const;

    /**
     * Calculates the brightness value of the closest step downwards.
     * (Closest step that is lower than current brightness value).
     *
     * @return The brightness value of the closest step downwards
     */
    int decreased() const;

    /**
     * Calculates the brightness value of the toggled state.
     * (Sets the brightness value to either minimum, last active brightness or maximum).
     *
     * @return The brightness value that should be set, or -1 if nothing should be done
     */
    virtual int toggled() const;

    /**
     * Calculates the brightness value of the closest step upwards.
     * (Closest step that is higher than current brightness value).
     *
     * @return The brightness value of the closest step upwards
     */
    int increasedSmall() const;

    /**
     * Calculates the brightness value of the closest step downwards.
     * (Closest step that is lower than current brightness value).
     *
     * @return The brightness value of the closest step downwards
     */
    int decreasedSmall() const;

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
    BrightnessInfo m_current;
};

}
