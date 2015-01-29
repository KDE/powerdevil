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
 : m_value(-1), m_valueMax(-1), m_steps(-1)
{
}

BrightnessLogic::~BrightnessLogic()
{
}

void BrightnessLogic::setValue(int value) {
    m_value = value;
}

void BrightnessLogic::setValueMax(int valueMax) {
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
    }

    return -1; // We shouldn't get here
}

int BrightnessLogic::increased() const {
    if (m_value == m_valueMax) {
        return -1; // ignore, we are at the maximum
    }

    // Add 1 and round upwards to the nearest step
    int step = m_steps - (m_valueMax - m_value - 1) * m_steps / m_valueMax;
    return stepToValue(step);
}

int BrightnessLogic::decreased() const {
    if (m_value == 0) {
        return -1; // ignore, we are at the minimum
    }

    // Subtract 1 and round downwards to the nearest Step
    int step = (m_value - 1) * m_steps / m_valueMax;
    return stepToValue(step);
}

int BrightnessLogic::toggled() const {
    // If it's not minimum, set to minimum, if it's minimum, set to maximum
    return m_value > 0 ? 0 : m_valueMax;
}

int BrightnessLogic::value() const {
    return m_value;
}

int BrightnessLogic::valueMax() const {
    return m_valueMax;
}

int BrightnessLogic::steps() const {
    return m_steps;
}

float BrightnessLogic::percentage() const {
    return m_value * 100.0 / m_valueMax;
}

const BrightnessLogic::BrightnessInfo BrightnessLogic::info() const {
    BrightnessInfo brightnessInfo;
    brightnessInfo.valueMax = m_valueMax;
    brightnessInfo.value = m_value;
    brightnessInfo.steps = m_steps;
    brightnessInfo.percentage = percentage();
    return brightnessInfo;
}

int BrightnessLogic::stepToValue(int step) const {
    return qBound(0, qRound(step * 1.0 * m_valueMax / m_steps), m_valueMax);
}

int BrightnessLogic::valueToStep(int value) const {
    return qBound(0, qRound(value * 1.0 * m_steps / m_valueMax), m_steps);
}

}
