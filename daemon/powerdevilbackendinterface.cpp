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
#include "powerdevilscreenbrightnesslogic.h"
#include "powerdevilkeyboardbrightnesslogic.h"
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

class BackendInterface::Private
{
public:
    Private()
        : acAdapterState(UnknownAcAdapterState)
        , batteryRemainingTime(0)
        , isReady(false)
        , isError(false)
        , isLidClosed(false)
        , isLidPresent(false)
    {
    }
    ~Private() {}

    AcAdapterState acAdapterState;

    qulonglong batteryRemainingTime;
    qulonglong smoothedBatteryRemainingTime = 0;
    qulonglong lastRateTimestamp = 0;
    double batteryEnergyFull = 0;
    double batteryEnergy = 0;
    double smoothedBatteryDischargeRate = 0;

    ScreenBrightnessLogic screenBrightnessLogic;
    KeyboardBrightnessLogic keyboardBrightnessLogic;
    Capabilities capabilities;
    SuspendMethods suspendMethods;
    QString errorString;
    bool isReady;
    bool isError;
    bool isLidClosed;
    bool isLidPresent;
};

BackendInterface::BackendInterface(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

BackendInterface::~BackendInterface()
{
    delete d;
}

BackendInterface::AcAdapterState BackendInterface::acAdapterState() const
{
    return d->acAdapterState;
}

qulonglong BackendInterface::batteryRemainingTime() const
{
    return d->batteryRemainingTime;
}

qulonglong BackendInterface::smoothedBatteryRemainingTime() const
{
    return d->smoothedBatteryRemainingTime;
}

int BackendInterface::screenBrightnessSteps() const
{
    d->screenBrightnessLogic.setValueMax(screenBrightnessMax());
    return d->screenBrightnessLogic.steps();
}

int BackendInterface::keyboardBrightnessSteps() const
{
    d->keyboardBrightnessLogic.setValueMax(keyboardBrightnessMax());
    return d->keyboardBrightnessLogic.steps();
}

BackendInterface::SuspendMethods BackendInterface::supportedSuspendMethods() const
{
    return d->suspendMethods;
}

bool BackendInterface::isLidClosed() const
{
    return d->isLidClosed;
}

bool BackendInterface::isLidPresent() const
{
    return d->isLidPresent;
}

void BackendInterface::setLidPresent(bool present)
{
    d->isLidPresent = present;
}

void BackendInterface::setAcAdapterState(PowerDevil::BackendInterface::AcAdapterState state)
{
    d->acAdapterState = state;
    Q_EMIT acAdapterStateChanged(state);
}

void BackendInterface::setBackendHasError(const QString& errorDetails)
{
    Q_UNUSED(errorDetails)
}

void BackendInterface::setBackendIsReady(BackendInterface::SuspendMethods supportedSuspendMethods)
{
    d->suspendMethods = supportedSuspendMethods;
    d->isReady = true;

    Q_EMIT backendReady();
}

void BackendInterface::setBatteryEnergyFull(const double energy)
{
    d->batteryEnergyFull = energy;
}

void BackendInterface::setBatteryEnergy(const double energy)
{
    d->batteryEnergy = energy;
}

void BackendInterface::setBatteryRate(const double rate, qulonglong timestamp)
{
    // remaining time in milliseconds
    qulonglong time = 0;

    if (rate > 0) {
        // Energy and rate are in Watt*hours resp. Watt
        time = 3600 * 1000 * (d->batteryEnergyFull - d->batteryEnergy) / rate;
    } else if (rate < 0) {
        time = 3600 * 1000 * (0.0 - d->batteryEnergy) / rate;
    }

    if (d->batteryRemainingTime != time) {
        d->batteryRemainingTime = time;
        Q_EMIT batteryRemainingTimeChanged(time);
    }

    // Charging or full
    if ((rate > 0) || (time == 0)) {
        if (d->smoothedBatteryRemainingTime != time) {
            d->smoothedBatteryRemainingTime = time;
            Q_EMIT smoothedBatteryRemainingTimeChanged(time);
        }
        return;
    }

    double oldRate = d->smoothedBatteryDischargeRate;
    if (oldRate == 0) {
        d->smoothedBatteryDischargeRate = rate;
    } else {
        // To have a time constant independent from the update frequency
        // the weight must be scaled
        double weight = 0.005 * std::min<qulonglong>(60, timestamp - d->lastRateTimestamp);
        d->lastRateTimestamp = timestamp;
        d->smoothedBatteryDischargeRate = emafilter(oldRate, rate, weight);
    }

    time = 3600 * 1000 * (0.0 - d->batteryEnergy) / d->smoothedBatteryDischargeRate;

    if (d->smoothedBatteryRemainingTime != time) {
        d->smoothedBatteryRemainingTime = time;
        Q_EMIT smoothedBatteryRemainingTimeChanged(d->smoothedBatteryRemainingTime);
    }
}

void BackendInterface::setButtonPressed(PowerDevil::BackendInterface::ButtonType type)
{
    if (type == LidClose && !d->isLidClosed) {
        d->isLidClosed = true;
        Q_EMIT lidClosedChanged(true);
    } else if (type == LidOpen && d->isLidClosed) {
        d->isLidClosed = false;
        Q_EMIT lidClosedChanged(false);
    }
    Q_EMIT buttonPressed(type);
}

void BackendInterface::onScreenBrightnessChanged(int value, int valueMax)
{
    d->screenBrightnessLogic.setValueMax(valueMax);
    d->screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(d->screenBrightnessLogic.info());
}

void BackendInterface::onKeyboardBrightnessChanged(int value, int valueMax)
{
    d->keyboardBrightnessLogic.setValueMax(valueMax);
    d->keyboardBrightnessLogic.setValue(value);

    Q_EMIT keyboardBrightnessChanged(d->keyboardBrightnessLogic.info());
}

BackendInterface::Capabilities BackendInterface::capabilities() const
{
    return d->capabilities;
}

void BackendInterface::setCapabilities(BackendInterface::Capabilities capabilities)
{
    d->capabilities = capabilities;
}

int BackendInterface::calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    d->screenBrightnessLogic.setValueMax(valueMax);
    d->screenBrightnessLogic.setValue(value);

    return d->screenBrightnessLogic.action(keyType);
}

int BackendInterface::calculateNextKeyboardBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    d->keyboardBrightnessLogic.setValueMax(valueMax);
    d->keyboardBrightnessLogic.setValue(value);

    return d->keyboardBrightnessLogic.action(keyType);
}

}
