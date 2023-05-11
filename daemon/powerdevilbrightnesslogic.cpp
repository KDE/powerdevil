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

#include "powerdevilbrightnesslogic.h"
#include "powerdevilbackendinterface.h"

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

int BrightnessLogic::action(BrightnessKeyType type) const {
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
    if (m_value == 0) {
        return 0; // we are at the minimum already
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
    return std::max<int>(0, std::round(m_valueMax * newPercent));
}

int BrightnessLogic::toggled() const
{
    // If it's not minimum, set to minimum, if it's minimum, set to maximum
    return m_value > 0 ? 0 : m_valueMax;
}

int BrightnessLogic::value() const
{
    return m_value;
}

int BrightnessLogic::valueMax() const
{
    return m_valueMax;
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
    return BrightnessInfo{
        m_value, m_valueMax, m_steps
    };
}

int BrightnessLogic::stepToValue(int step) const
{
    return qBound(0, qRound(step * 1.0 * m_valueMax / m_steps), m_valueMax);
}

int BrightnessLogic::valueToStep(int value) const
{
    return qBound(0, qRound(value * 1.0 * m_steps / m_valueMax), m_steps);
}

}
