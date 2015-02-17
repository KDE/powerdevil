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

class BackendInterface::Private
{
public:
    Private() : batteryRemainingTime(0), isReady(false), isError(false), isLidClosed(false), isLidPresent(false) {}
    ~Private() {}

    AcAdapterState acAdapterState;
    qulonglong batteryRemainingTime;
    BatteryState batteryState;
    QHash< BrightnessControlType, BrightnessLogic* > brightnessLogic;
    BrightnessControlsList brightnessControlsAvailable;
    Capabilities capabilities;
    SuspendMethods suspendMethods;
    QString errorString;
    bool isReady;
    bool isError;
    bool isLidClosed;
    bool isLidPresent;
    QHash< QString, uint > capacities;
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

QHash< QString, uint > BackendInterface::capacities() const
{
    return d->capacities;
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
    emit acAdapterStateChanged(state);
}

void BackendInterface::setBackendHasError(const QString& errorDetails)
{
    Q_UNUSED(errorDetails)
}

void BackendInterface::setBackendIsReady(BrightnessControlsList availableBrightnessControls,
                                         BackendInterface::SuspendMethods supportedSuspendMethods)
{
    d->brightnessControlsAvailable = availableBrightnessControls;
    d->suspendMethods = supportedSuspendMethods;
    d->isReady = true;

    emit backendReady();
}

void BackendInterface::setBatteryRemainingTime(qulonglong time)
{
    if (d->batteryRemainingTime != time) {
        d->batteryRemainingTime = time;
        emit batteryRemainingTimeChanged(time);
    }
}

void BackendInterface::setBatteryState(PowerDevil::BackendInterface::BatteryState state)
{
    d->batteryState = state;
    emit batteryStateChanged(state);
}

void BackendInterface::setButtonPressed(PowerDevil::BackendInterface::ButtonType type)
{
    if (type == LidClose) {
        d->isLidClosed = true;
    } else if (type == LidOpen) {
        d->isLidClosed = false;
    }
    emit buttonPressed(type);
}

void BackendInterface::setCapacityForBattery(const QString& batteryId, uint percent)
{
    d->capacities.insert(batteryId, percent);
}

void BackendInterface::onBrightnessChanged(BrightnessControlType type, int value, int valueMax)
{
    BrightnessLogic *logic = d->brightnessLogic.value(type);
    logic->setValueMax(valueMax);
    logic->setValue(value);

    emit brightnessChanged(logic->info(), type);
}

void BackendInterface::setResumeFromSuspend()
{
    emit resumeFromSuspend();
}

BackendInterface::Capabilities BackendInterface::capabilities() const
{
    return d->capabilities;
}

void BackendInterface::setCapabilities(BackendInterface::Capabilities capabilities)
{
    d->capabilities = capabilities;
}

int BackendInterface::calculateNextStep(int value, int valueMax, BrightnessControlType type, BrightnessLogic::BrightnessKeyType keyType)
{
    BrightnessLogic *logic = d->brightnessLogic.value(type);
    logic->setValueMax(valueMax);
    logic->setValue(value);

    return logic->action(keyType);
}

}
