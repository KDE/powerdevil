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

void BrightnessLogic::setValue(PerceivedBrightness value)
{
    m_value = value;
}

void BrightnessLogic::setValueMax(int valueMax)
{
    if (valueMax != m_valueMax) {
        m_valueMax = PerceivedBrightness(valueMax);
        m_maxStep = calculateSteps(PerceivedBrightness(valueMax));
    }
}

PerceivedBrightness BrightnessLogic::action(BrightnessKeyType type) const {
    switch (type) {
    case Increase:
        return increased();
    case Decrease:
        return decreased();
    case Toggle:
        return toggled();
    }
    Q_UNREACHABLE();
}

PerceivedBrightness BrightnessLogic::increased() const
{
    if (m_value >= m_valueMax) {
        return m_valueMax; // we are at the maximum already
    }

    ActualBrightness current = ActualBrightness(m_value, (int)m_valueMax);

    // Jump to the next whole step
    Step step = m_value < stepToValue(closestStep()) ? closestStep() : closestStep() + 1;
    // Because we're using integers and converting between perceived and Actual
    // brightness values is continuous, we check here that the actual brightness
    // value has changed and increase it if it hasn't
    if ((int)current >= (int)ActualBrightness(stepToValue(step), (int)m_valueMax)) {
        step++;
    }

    return stepToValue(step);
}

PerceivedBrightness BrightnessLogic::decreased() const
{
    if (m_value <= 0_pb) {
        return 0_pb; // we are at the minimum already
    }

    ActualBrightness current = ActualBrightness(m_value, (int)m_valueMax);

    // Jump to the next whole step
    Step step = m_value > stepToValue(closestStep()) ? closestStep() : closestStep() - 1;
    // Because we're using integers and converting between perceived and Actual
    // brightness values is continuous, we check here that the actual brightness
    // value has changed and decrease it if it hasn't
    if ((int)current <= (int)ActualBrightness(stepToValue(step), (int)m_valueMax)) {
        step--;
    }

    return stepToValue(step);
}

PerceivedBrightness BrightnessLogic::toggled() const
{
    // If it's not minimum, set to minimum, if it's minimum, set to maximum
    return m_value > 0_pb ? 0_pb : m_valueMax;
}

PerceivedBrightness BrightnessLogic::value() const
{
    return m_value;
}

int BrightnessLogic::valueMax() const
{
    return (int)m_valueMax;
}

BrightnessLogic::Step BrightnessLogic::closestStep() const
{
    Q_ASSERT((int)m_value >= 0 && m_value <= m_valueMax);
    Q_ASSERT((int)m_valueMax >= 0);
    Q_ASSERT(m_maxStep >= 0);
    double brightness = (double)(int)m_value / (int)m_valueMax;
    return std::clamp(std::round(brightness * m_maxStep), 0.0, (double)m_maxStep);
}

BrightnessLogic::Step BrightnessLogic::steps() const
{
    return m_maxStep;
}

float BrightnessLogic::percentage(PerceivedBrightness value) const
{
    return (int)value * 100.0 / (int)m_valueMax;
}

const BrightnessLogic::BrightnessInfo BrightnessLogic::info() const
{
    return BrightnessInfo{
        ActualBrightness(m_value, (int)m_valueMax), (int)m_valueMax, m_maxStep
    };
}

PerceivedBrightness BrightnessLogic::stepToValue(Step step) const
{
    return PerceivedBrightness(qBound(0, qRound((step * 1.0 / m_maxStep) * (int)m_valueMax), (int)m_valueMax));
}

BrightnessLogic::Step BrightnessLogic::valueToStep(PerceivedBrightness value) const
{
    return Step(qBound(0, qRound((int)value * 1.0 * m_maxStep / (int)m_valueMax), m_maxStep));
}

}
