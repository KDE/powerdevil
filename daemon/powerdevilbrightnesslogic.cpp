/*
 *   SPDX-FileCopyrightText: 2014 Nikita Skovoroda <chalkerx@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilbrightnesslogic.h"
#include "powerdevilbackendinterface.h"

#include <QDebug>

namespace PowerDevil
{
BrightnessLogic::BrightnessLogic()
{
}

void BrightnessLogic::setValue(int value)
{
    m_value = value;
}

void BrightnessLogic::setValueMax(int valueMax)
{
    if (valueMax != m_valueMax) {
        m_valueMax = valueMax;
        m_steps = calculateSteps(valueMax);
    }
}

void BrightnessLogic::setValueBeforeTogglingOff(int valueBeforeTogglingOff)
{
    m_valueBeforeTogglingOff = valueBeforeTogglingOff;
}

int BrightnessLogic::action(BrightnessKeyType type) const
{
    switch (type) {
    case Increase:
        return increased();
    case Decrease:
        return decreased();
    case Toggle:
        return toggled();
    case IncreaseSmall:
        return increasedSmall();
    case DecreaseSmall:
        return decreasedSmall();
    }

    return -1; // We shouldn't get here
}

int BrightnessLogic::increased() const
{
    if (m_value == m_valueMax) {
        return m_valueMax; // we are at the maximum already
    }

    // Add 1 and round upwards to the nearest step
    int step = m_steps - (m_valueMax - m_value - 1) * m_steps / m_valueMax;

    if (m_valueMax > 100 && qRound(percentage(stepToValue(step))) <= qRound(percentage(m_value))) {
        // When no visible change was made, add 1 step.
        // This can happen only if valueMax > 100, else 1 >= 1%.
        step++;
    }

    return stepToValue(step);
}

int BrightnessLogic::increasedSmall() const
{
    const double newPercent = (std::round(m_value * 100 / double(m_valueMax)) + 1) / 100.0;
    return std::min<int>(m_valueMax, std::round(m_valueMax * newPercent));
}

int BrightnessLogic::decreased() const
{
    if (m_value == valueMin()) {
        return valueMin(); // we are at the minimum already
    }

    // Subtract 1 and round downwards to the nearest Step
    int step = (m_value - 1) * m_steps / m_valueMax;

    if (m_valueMax > 100 && qRound(percentage(stepToValue(step))) >= qRound(percentage(m_value))) {
        // When no visible change was made, subtract 1 step.
        // This can happen only if valueMax > 100, else 1 >= 1%.
        step--;
    }

    return stepToValue(step);
}

int BrightnessLogic::decreasedSmall() const
{
    const double newPercent = (std::round(m_value * 100 / double(m_valueMax)) - 1) / 100.0;
    return std::max<int>(valueMin(), std::round(m_valueMax * newPercent));
}

int BrightnessLogic::toggled() const
{
    if (m_value > 0) {
        return 0; // currently on: toggle off
    } else if (m_valueBeforeTogglingOff > 0) {
        return m_valueBeforeTogglingOff; // currently off and was on before toggling: restore
    } else {
        return m_valueMax; // currently off and would stay off if restoring: toggle to max
    }
}

int BrightnessLogic::steps() const
{
    return m_steps;
}

float BrightnessLogic::percentage(int value) const
{
    return value * 100.0 / m_valueMax;
}

const BrightnessLogic::BrightnessInfo BrightnessLogic::info() const
{
    return BrightnessInfo{m_value, m_valueMax, m_valueBeforeTogglingOff, m_steps};
}

int BrightnessLogic::stepToValue(int step) const
{
    return qBound(valueMin(), qRound(step * 1.0 * m_valueMax / m_steps), m_valueMax);
}

}
