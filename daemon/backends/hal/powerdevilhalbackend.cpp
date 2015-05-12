/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/


#include "powerdevilhalbackend.h"
#include <powerdevil_debug.h>

#include <QTimer>
#include <QDBusReply>
#include <QDBusConnectionInterface>
#include <QDebug>

#include <Solid/Battery>
#include <Solid/GenericInterface>
#include <Solid/DeviceNotifier>

PowerDevilHALBackend::PowerDevilHALBackend(QObject* parent)
    : BackendInterface(parent),
      m_screenBrightnessInHardware(false),
      m_halComputer("org.freedesktop.Hal",
                     "/org/freedesktop/Hal/devices/computer",
                     "org.freedesktop.Hal.Device",
                     QDBusConnection::systemBus()),
      m_halPowerManagement("org.freedesktop.Hal",
                            "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement",
                            QDBusConnection::systemBus()),
      m_halCpuFreq("org.freedesktop.Hal",
                    "/org/freedesktop/Hal/devices/computer",
                    "org.freedesktop.Hal.Device.CPUFreq",
                    QDBusConnection::systemBus()),
      m_halManager("org.freedesktop.Hal",
                    "/org/freedesktop/Hal/Manager",
                    "org.freedesktop.Hal.Manager",
                    QDBusConnection::systemBus())
{
}

PowerDevilHALBackend::~PowerDevilHALBackend()
{
    qDeleteAll(m_batteries);
}

bool PowerDevilHALBackend::isAvailable()
{
    return QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.Hal");
}

void PowerDevilHALBackend::init()
{
    setCapabilities(SignalResumeFromSuspend); // not really, but we fake it

    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
            this, SLOT(slotDeviceRemoved(QString)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
            this, SLOT(slotDeviceAdded(QString)));

    computeBatteries();
    updateBatteryStats();

    // Brightness Control available
    BrightnessControlsList controls;
    {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if (reply.isValid()) {
            foreach(const QString &name, reply.value()) {
                controls.insert(name, Screen);
            }
        }
        reply = m_halManager.call("FindDeviceByCapability", "keyboard_backlight");
        if (reply.isValid()) {
            foreach(const QString &name, reply.value()) {
                controls.insert(name, Keyboard);
            }
        }
    }

    const QStringList screenControls = controls.keys(Screen);

    if (!screenControls.isEmpty()) {
        m_cachedScreenBrightness = brightness(Screen);

        QDBusInterface deviceInterface("org.freedesktop.Hal",
                                       screenControls.at(0),
                                       "org.freedesktop.Hal.Device",
                                       QDBusConnection::systemBus());
        QDBusReply<bool> replyInHardware = deviceInterface.call("GetPropertyBoolean",
                                                                "laptop_panel.brightness_in_hardware");
        if (replyInHardware.isValid()) {
            m_screenBrightnessInHardware = replyInHardware;
        }
    }

    // Battery capacity
    {
        foreach (Solid::Device *d, m_batteries) {
            Solid::GenericInterface *interface = d->as<Solid::GenericInterface>();

            if (interface == 0) continue;

            if (interface->property("battery.reporting.design").toInt() > 0) {
                uint capacity = ((float)(interface->property("battery.reporting.last_full").toInt()) /
                                 (float)(interface->property("battery.reporting.design").toInt())) * 100;

                if (capacity > 0) {
                    setCapacityForBattery(d->udi(), capacity);
                } else {
                    // Not supported, set capacity to 100%
                    setCapacityForBattery(d->udi(), 100);
                }
            } else {
                // Not supported, set capacity to 100%
                setCapacityForBattery(d->udi(), 100);
            }
        }
    }

    setBackendIsReady(controls);
}

int PowerDevilHALBackend::brightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type, BrightnessControlType controlType)
{
    const QStringList controls = brightnessControlsAvailable().keys(controlType);

    if (controls.isEmpty()) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    if (type == PowerDevil::BrightnessLogic::Toggle && controlType == Screen) {
        return -1; // ignore as we won't toggle the display off
    }

    int currentBrightness = brightness(controlType);

    int cachedBrightness;

    if (controlType == Screen) {
        cachedBrightness = m_cachedScreenBrightness;
    } else {
        cachedBrightness = m_cachedKeyboardBrightness;
    }

    if ((currentBrightness == cachedBrightness) && (!m_screenBrightnessInHardware || controlType == Screen)) {
        int maxBrightness = brightnessMax(controlType),
            newBrightness = calculateNextStep(currentBrightness, maxBrightness, controlType, type);

        if (newBrightness >= 0) {
            setBrightness(newBrightness, controlType);
            newBrightness = brightness(controlType);
            if (newBrightness != cachedBrightness) {
                cachedBrightness = newBrightness;
                onBrightnessChanged(controlType, newBrightness, maxBrightness);
            }
        }
    } else {
        cachedBrightness = currentBrightness;
    }

    if (controlType == Screen) {
        m_cachedScreenBrightness = cachedBrightness;
    } else {
        m_cachedKeyboardBrightness = cachedBrightness;
    }

    return cachedBrightness;
}

int PowerDevilHALBackend::brightness(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    if (type == Screen) {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                               "org.freedesktop.Hal.Device.LaptopPanel", QDBusConnection::systemBus());
                QDBusReply<int> brightnessReply = deviceInterface.call("GetBrightness");
                if (brightnessReply.isValid()) {
                    return brightnessReply.value();
                }
            }
        }
    } else {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "keyboard_backlight");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                //TODO - I do not have a backlight enabled keyboard, so I'm guessing a bit here. Could someone please check this.
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                               "org.freedesktop.Hal.Device.KeyboardBacklight", QDBusConnection::systemBus());

                QDBusReply<int> brightnessReply = deviceInterface.call("GetBrightness");
                if (brightnessReply.isValid()) {
                    return brightnessReply.value();
                }
            }
        }
    }
    return 0;
}

int PowerDevilHALBackend::brightnessMax(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    if (type == Screen) {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                 "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                QDBusReply<int> levelsReply = propertyInterface.call("GetProperty", "laptop_panel.num_levels");
                if (levelsReply.isValid()) {
                    return levelsReply.value();
                }
            }
        }
    } else {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "keyboard_backlight");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                //TODO - I do not have a backlight enabled keyboard, so I'm guessing a bit here. Could someone please check this.
                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                 "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                QDBusReply<int> levelsReply = propertyInterface.call("GetProperty", "keyboard_backlight.num_levels");
                if (levelsReply.isValid()) {
                    return levelsReply.value();
                }
            }
        }
    }
    return 0;
}

void PowerDevilHALBackend::setBrightness(int value, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                            "org.freedesktop.Hal.Device.LaptopPanel", QDBusConnection::systemBus());
                deviceInterface.call("SetBrightness", value);
                if (!deviceInterface.lastError().isValid()) {
                    return;
                }
            }
        }
    } else {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "keyboard_backlight");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                //TODO - I do not have a backlight enabled keyboard, so I'm guessing a bit here. Could someone please check this.
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                               "org.freedesktop.Hal.Device.KeyboardBacklight", QDBusConnection::systemBus());

                deviceInterface.call("SetBrightness", value);
                if(!deviceInterface.lastError().isValid()) {
                    return;
                }
            }
        }
    }
    return;
}

void PowerDevilHALBackend::computeBatteries()
{
    const QList<Solid::Device> batteries = Solid::Device::listFromQuery("Battery.type == 'PrimaryBattery'");

    foreach (const Solid::Device &battery, batteries) {
        m_batteries[battery.udi()] = new Solid::Device(battery);
        connect(m_batteries[battery.udi()]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(updateBatteryStats()));
        connect(m_batteries[battery.udi()]->as<Solid::GenericInterface>(), SIGNAL(propertyChanged(QMap<QString,int>)),
                 this, SLOT(slotBatteryPropertyChanged(QMap<QString,int>)));
    }

    updateBatteryStats();
}

void PowerDevilHALBackend::slotDeviceAdded(const QString &udi)
{
    Solid::Device *device = new Solid::Device(udi);
    if (device->is<Solid::Battery>()) {
        m_batteries[udi] = device;
        connect(m_batteries[udi]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(updateBatteryStats()));
        connect(m_batteries[udi]->as<Solid::GenericInterface>(), SIGNAL(propertyChanged(QMap<QString,int>)),
                 this, SLOT(slotBatteryPropertyChanged(QMap<QString,int>)));
    } else {
        delete device;
    }
}

void PowerDevilHALBackend::slotDeviceRemoved(const QString &udi)
{
    Solid::Device *device = 0;

    device = m_batteries.take(udi);

    if (device!=0) {
        delete device;
        updateBatteryStats();
        return;
    }
}

void PowerDevilHALBackend::slotBatteryPropertyChanged(const QMap<QString,int> &changes)
{
    /* This slot catches property changes on battery devices. At
     * the moment it is used to find out remaining time on batteries.
     */

    if (changes.contains("battery.remaining_time")) {
        updateBatteryStats();

        setBatteryRemainingTime(m_estimatedBatteryTime);
    }
}

void PowerDevilHALBackend::updateBatteryStats()
{
    m_currentBatteryCharge = 0;
    m_maxBatteryCharge = 0;
    m_lowBatteryCharge = 0;
    m_criticalBatteryCharge = 0;
    m_estimatedBatteryTime = 0;

    foreach (Solid::Device *d, m_batteries) {
        Solid::GenericInterface *interface = d->as<Solid::GenericInterface>();

        if (interface == 0) continue;

        m_currentBatteryCharge += interface->property("battery.charge_level.current").toInt();
        m_maxBatteryCharge += interface->property("battery.charge_level.last_full").toInt();
        m_lowBatteryCharge += interface->property("battery.charge_level.low").toInt();
        m_estimatedBatteryTime += interface->property("battery.remaining_time").toInt() * 1000;
    }

    m_criticalBatteryCharge = m_lowBatteryCharge/2;
}
