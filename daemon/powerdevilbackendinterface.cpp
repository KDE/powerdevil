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
    Private() : acAdapterState(UnknownAcAdapterState), batteryState(NoBatteryState), batteryRemainingTime(0),
                isReady(false), isError(false), isLidClosed(false), isLidPresent(false) {}
    ~Private() {}

    AcAdapterState acAdapterState;
    BatteryState batteryState;
    qulonglong batteryRemainingTime;
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

void BackendInterface::setBrightness(ActualBrightness brightness, BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        qCDebug(POWERDEVIL) << "set screen brightness: " << (QString)brightness;
    } else {
        qCDebug(POWERDEVIL) << "set kbd backlight: " << (QString)brightness;
    }

    d->brightnessLogic.value(type)->setValue(PerceivedBrightness(brightness, brightnessMax(type)));
}

ActualBrightness BackendInterface::brightness(BackendInterface::BrightnessControlType type) const
{
    return ActualBrightness(d->brightnessLogic.value(type)->value(), brightnessMax(type));
}

PerceivedBrightness BackendInterface::perceivedBrightness(BackendInterface::BrightnessControlType type) const
{
    return PerceivedBrightness(brightness(type), brightnessMax(type));
}

int BackendInterface::brightnessMax(BackendInterface::BrightnessControlType type) const
{
    return d->brightnessLogic.value(type)->valueMax();
}

BrightnessLogic::Step BackendInterface::brightnessSteps(BackendInterface::BrightnessControlType type) const
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
    Q_EMIT acAdapterStateChanged(state);
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

void BackendInterface::setCapacityForBattery(const QString& batteryId, uint percent)
{
    d->capacities.insert(batteryId, percent);
}

void BackendInterface::onBrightnessChanged(BrightnessControlType type, ActualBrightness value, int valueMax)
{
    BrightnessLogic *logic = d->brightnessLogic.value(type);
    logic->setValueMax(valueMax);
    logic->setValue(PerceivedBrightness(value, valueMax));

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

ActualBrightness BackendInterface::calculateNextStep(ActualBrightness current, int valueMax, BrightnessControlType controlType, BrightnessLogic::BrightnessKeyType keyType) const
{
    BrightnessLogic *logic = d->brightnessLogic.value(controlType);
    logic->setValueMax(valueMax);
    logic->setValue(PerceivedBrightness(current, valueMax));

    return ActualBrightness(logic->action(keyType), valueMax);
}

}
