/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#include "powerdevilbackendinterface.h"
#include "powerdevil_debug.h"
#include <QDebug>

namespace PowerDevil
{
/**
 * Filter data along one dimension using exponential moving average.
 */
double emafilter(const double last, const double update, double weight)
{
    double current = last * (1 - weight) + update * weight;

    return current;
}

BackendInterface::BackendInterface(QObject *parent)
    : QObject(parent)
{
}

BackendInterface::~BackendInterface()
{
}

BackendInterface::AcAdapterState BackendInterface::acAdapterState() const
{
    return m_acAdapterState;
}

qulonglong BackendInterface::batteryRemainingTime() const
{
    return m_batteryRemainingTime;
}

qulonglong BackendInterface::smoothedBatteryRemainingTime() const
{
    return m_smoothedBatteryRemainingTime;
}

int BackendInterface::screenBrightnessSteps()
{
    m_screenBrightnessLogic.setValueMax(screenBrightnessMax());
    return m_screenBrightnessLogic.steps();
}

int BackendInterface::keyboardBrightnessSteps()
{
    m_keyboardBrightnessLogic.setValueMax(keyboardBrightnessMax());
    return m_keyboardBrightnessLogic.steps();
}

BackendInterface::SuspendMethods BackendInterface::supportedSuspendMethods() const
{
    return m_suspendMethods;
}

bool BackendInterface::isLidClosed() const
{
    return m_isLidClosed;
}

QBindable<bool> BackendInterface::bindableIsLidClosed() const
{
    return &m_isLidClosed;
}

bool BackendInterface::isLidPresent() const
{
    return m_isLidPresent;
}

QBindable<bool> BackendInterface::bindableIsLidPresent() const
{
    return &m_isLidPresent;
}

void BackendInterface::setLidPresent(bool present)
{
    m_isLidPresent = present;
}

QBindable<bool> BackendInterface::isSessionActive() const
{
    return &m_isSessionActive;
}

QBindable<bool> BackendInterface::isSessionIdle() const
{
    return &m_isSessionIdle;
}

void BackendInterface::setAcAdapterState(PowerDevil::BackendInterface::AcAdapterState state)
{
    m_acAdapterState = state;
    Q_EMIT acAdapterStateChanged(state);
}

void BackendInterface::setBackendHasError(const QString &errorDetails)
{
    Q_UNUSED(errorDetails)
}

void BackendInterface::setBackendIsReady(BackendInterface::SuspendMethods supportedSuspendMethods)
{
    m_suspendMethods = supportedSuspendMethods;

    Q_EMIT backendReady();
}

void BackendInterface::setBatteryEnergyFull(const double energy)
{
    m_batteryEnergyFull = energy;
}

void BackendInterface::setBatteryEnergy(const double energy)
{
    m_batteryEnergy = energy;
}

void BackendInterface::setBatteryRate(const double rate, qulonglong timestamp)
{
    // remaining time in milliseconds
    qulonglong time = 0;

    if (rate > 0) {
        // Energy and rate are in Watt*hours resp. Watt
        time = 3600 * 1000 * (m_batteryEnergyFull - m_batteryEnergy) / rate;
    } else if (rate < 0) {
        time = 3600 * 1000 * (0.0 - m_batteryEnergy) / rate;
    }

    if (m_batteryRemainingTime != time) {
        m_batteryRemainingTime = time;
        Q_EMIT batteryRemainingTimeChanged(time);
    }

    // Charging or full
    if ((rate > 0) || (time == 0)) {
        if (m_smoothedBatteryRemainingTime != time) {
            m_smoothedBatteryRemainingTime = time;
            Q_EMIT smoothedBatteryRemainingTimeChanged(time);
        }
        return;
    }

    double oldRate = m_smoothedBatteryDischargeRate;
    if (oldRate == 0) {
        m_smoothedBatteryDischargeRate = rate;
    } else {
        // To have a time constant independent from the update frequency
        // the weight must be scaled
        double weight = 0.005 * std::min<qulonglong>(60, timestamp - m_lastRateTimestamp);
        m_lastRateTimestamp = timestamp;
        m_smoothedBatteryDischargeRate = emafilter(oldRate, rate, weight);
    }

    time = 3600 * 1000 * (0.0 - m_batteryEnergy) / m_smoothedBatteryDischargeRate;

    if (m_smoothedBatteryRemainingTime != time) {
        m_smoothedBatteryRemainingTime = time;
        Q_EMIT smoothedBatteryRemainingTimeChanged(m_smoothedBatteryRemainingTime);
    }
}

void BackendInterface::setButtonPressed(PowerDevil::BackendInterface::ButtonType type)
{
    if (type == LidClose && !m_isLidClosed) {
        m_isLidClosed = true;
        Q_EMIT lidClosedChanged(true);
    } else if (type == LidOpen && m_isLidClosed) {
        m_isLidClosed = false;
        Q_EMIT lidClosedChanged(false);
    }
    Q_EMIT buttonPressed(type);
}

void BackendInterface::onScreenBrightnessChanged(int value, int valueMax)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

void BackendInterface::onKeyboardBrightnessChanged(int value, int valueMax)
{
    m_keyboardBrightnessLogic.setValueMax(valueMax);
    m_keyboardBrightnessLogic.setValue(value);

    Q_EMIT keyboardBrightnessChanged(m_keyboardBrightnessLogic.info());
}

BackendInterface::Capabilities BackendInterface::capabilities() const
{
    return m_capabilities;
}

void BackendInterface::setCapabilities(BackendInterface::Capabilities capabilities)
{
    m_capabilities = capabilities;
}

int BackendInterface::calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    return m_screenBrightnessLogic.action(keyType);
}

int BackendInterface::calculateNextKeyboardBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    m_keyboardBrightnessLogic.setValueMax(valueMax);
    m_keyboardBrightnessLogic.setValue(value);

    return m_keyboardBrightnessLogic.action(keyType);
}

}

#include "moc_powerdevilbackendinterface.cpp"
