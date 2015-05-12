/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Lukáš Tinkl <lukas@kde.org>                     *
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

#include <Solid/Power/PowerManagement>

namespace PowerDevil
{

class BackendInterface::Private
{
public:
    Private() : batteryRemainingTime(0), isReady(false), isError(false)
    {
        // AC plugged state
        if (Solid::PowerManagement::appShouldConserveResources())
            acAdapterState = Unplugged;
        else
            acAdapterState = Plugged;

        // supported suspend methods
        if (Solid::PowerManagement::canSuspend()) {
            suspendMethods |= ToRam;
        }
        if (Solid::PowerManagement::canHibernate()) {
            suspendMethods |= ToDisk;
        }
        if (Solid::PowerManagement::canHybridSleep()) {
            suspendMethods |= HybridSuspend;
        }
    }
    ~Private() {}

    AcAdapterState acAdapterState;
    qulonglong batteryRemainingTime;
    BatteryState batteryState;
    QHash< BrightnessControlType, BrightnessLogic* > brightnessLogic;
    BrightnessControlsList brightnessControlsAvailable;
    Capabilities capabilities;
    SuspendMethods suspendMethods = UnknownSuspendMethod;
    QString errorString;
    bool isReady;
    bool isError;
    QHash< QString, uint > capacities;
};

BackendInterface::BackendInterface(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
    d->brightnessLogic[Screen] = new ScreenBrightnessLogic();
    d->brightnessLogic[Keyboard] = new KeyboardBrightnessLogic();

    // AC adapter state changes
    connect(Solid::PowerManagement::notifier(), &Solid::PowerManagement::Notifier::appShouldConserveResourcesChanged,
            [this](bool onBattery){setAcAdapterState(onBattery ? Unplugged : Plugged);});
    // lid closed changes
    connect(Solid::PowerManagement::notifier(), &Solid::PowerManagement::Notifier::isLidClosedChanged,
            [this](bool lidClosed){setButtonPressed(lidClosed ? LidClose : LidOpen);});
    // "resuming" signal
    connect(Solid::PowerManagement::notifier(), &Solid::PowerManagement::Notifier::resumingFromSuspend,
            this, &BackendInterface::resumeFromSuspend);
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

void BackendInterface::suspend(BackendInterface::SuspendMethod method)
{
    switch (method) {
    case Standby:
    case ToRam:
        Solid::PowerManagement::suspend();
        break;
    case ToDisk:
        Solid::PowerManagement::hibernate();
        break;
    case HybridSuspend:
        Solid::PowerManagement::hybridSleep();
        break;
    case UnknownSuspendMethod:
    default:
        qCWarning(POWERDEVIL) << "Unknown suspend method:" << method;
        break;
    }
}

void BackendInterface::setAcAdapterState(PowerDevil::BackendInterface::AcAdapterState state)
{
    d->acAdapterState = state;
    emit acAdapterStateChanged(state);
}

void BackendInterface::setBackendHasError(const QString& errorDetails)
{
    d->errorString = errorDetails;
}

void BackendInterface::setBackendIsReady(BrightnessControlsList availableBrightnessControls)
{
    d->brightnessControlsAvailable = availableBrightnessControls;
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
