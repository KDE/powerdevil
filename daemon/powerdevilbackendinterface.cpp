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

using EnergyRateHistoryList = std::vector<double>;

/**
 * Filter data along one dimension using exponential moving average.
 */
qulonglong emafilter(const EnergyRateHistoryList &input)
{
    if (input.empty()) {
        return 0;
    }

    constexpr double weight = 0.05;
    double current = input[0];
    for (double n : input) {
        current = current * (1 - weight) + n * weight;
    }

    return current;
}

class BackendInterface::Private
{
public:
    Private()
        : acAdapterState(UnknownAcAdapterState)
        , batteryState(NoBatteryState)
        , batteryRemainingTime(0)
        , isReady(false)
        , isError(false)
        , isLidClosed(false)
        , isLidPresent(false)
    {
        energyRateHistoryRecords.reserve(64);
    }
    ~Private() {}

    AcAdapterState acAdapterState;
    BatteryState batteryState;
    qulonglong batteryRemainingTime;
    qulonglong smoothedBatteryRemainingTime = 0;
    EnergyRateHistoryList energyRateHistoryRecords;
    QHash< BrightnessControlType, BrightnessLogic* > brightnessLogic;
    BrightnessControlsList brightnessControlsAvailable;
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
    d->brightnessLogic[Screen] = new ScreenBrightnessLogic();
    d->brightnessLogic[Keyboard] = new KeyboardBrightnessLogic();
}

BackendInterface::~BackendInterface()
{
    delete d->brightnessLogic[Keyboard];
    delete d->brightnessLogic[Screen];
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

BackendInterface::BatteryState BackendInterface::batteryState() const
{
    return d->batteryState;
}

void BackendInterface::setBrightness(int brightness, BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        qCDebug(POWERDEVIL) << "set screen brightness: " << brightness;
    } else {
        qCDebug(POWERDEVIL) << "set kbd backlight: " << brightness;
    }

    d->brightnessLogic.value(type)->setValue(brightness);
}

int BackendInterface::brightness(BackendInterface::BrightnessControlType type) const
{
    return d->brightnessLogic.value(type)->value();
}

int BackendInterface::brightnessMax(BackendInterface::BrightnessControlType type) const
{
    return d->brightnessLogic.value(type)->valueMax();
}

int BackendInterface::brightnessSteps(BackendInterface::BrightnessControlType type) const
{
    BrightnessLogic *logic = d->brightnessLogic.value(type);
    logic->setValueMax(brightnessMax(type));
    return logic->steps();
}

BackendInterface::BrightnessControlsList BackendInterface::brightnessControlsAvailable() const
{
    return d->brightnessControlsAvailable;
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

    // AC state has been refreshed, so clear the history records
    d->energyRateHistoryRecords.clear();
    d->smoothedBatteryRemainingTime = 0;
    Q_EMIT smoothedBatteryRemainingTimeChanged(0);
}

void BackendInterface::setBackendHasError(const QString& errorDetails)
{
    Q_UNUSED(errorDetails)
}

void BackendInterface::setBackendIsReady(const BrightnessControlsList &availableBrightnessControls,
                                         BackendInterface::SuspendMethods supportedSuspendMethods)
{
    d->brightnessControlsAvailable = availableBrightnessControls;
    d->suspendMethods = supportedSuspendMethods;
    d->isReady = true;

    Q_EMIT backendReady();
}

void BackendInterface::setBatteryRemainingTime(qulonglong time)
{
    if (d->batteryRemainingTime != time) {
        d->batteryRemainingTime = time;
        Q_EMIT batteryRemainingTimeChanged(time);
    }

    if (d->acAdapterState == Plugged) {
        if (d->smoothedBatteryRemainingTime != time) {
            d->smoothedBatteryRemainingTime = time;
            Q_EMIT smoothedBatteryRemainingTimeChanged(time);
        }
    }
}

void BackendInterface::setSmoothedBatteryRemainingTime(double energyRateTotal, double energyTotal)
{
    if (d->acAdapterState == Plugged) {
        return;
    }

    const qulonglong oldValue = d->smoothedBatteryRemainingTime;
    if (energyRateTotal > 0) {
        if (d->energyRateHistoryRecords.size() == 64) {
            d->energyRateHistoryRecords.erase(d->energyRateHistoryRecords.begin());
        }
        d->energyRateHistoryRecords.emplace_back(energyRateTotal);
        const double smoothedEnergyRateTotal = emafilter(d->energyRateHistoryRecords);
        d->smoothedBatteryRemainingTime = energyTotal / smoothedEnergyRateTotal * 1000 * 3600;
    } else {
        // Don't let those samples pollute the history
        d->smoothedBatteryRemainingTime = d->batteryRemainingTime;
    }

    if (oldValue != d->smoothedBatteryRemainingTime) {
        Q_EMIT smoothedBatteryRemainingTimeChanged(d->smoothedBatteryRemainingTime);
    }
}

void BackendInterface::setBatteryState(PowerDevil::BackendInterface::BatteryState state)
{
    d->batteryState = state;
    Q_EMIT batteryStateChanged(state);
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

void BackendInterface::onBrightnessChanged(BrightnessControlType type, int value, int valueMax)
{
    BrightnessLogic *logic = d->brightnessLogic.value(type);
    logic->setValueMax(valueMax);
    logic->setValue(value);

    Q_EMIT brightnessChanged(logic->info(), type);
}

BackendInterface::Capabilities BackendInterface::capabilities() const
{
    return d->capabilities;
}

void BackendInterface::setCapabilities(BackendInterface::Capabilities capabilities)
{
    d->capabilities = capabilities;
}

int BackendInterface::calculateNextStep(int value, int valueMax, BrightnessControlType controlType, BrightnessLogic::BrightnessKeyType keyType)
{
    BrightnessLogic *logic = d->brightnessLogic.value(controlType);
    logic->setValueMax(valueMax);
    logic->setValue(value);

    return logic->action(keyType);
}

}
