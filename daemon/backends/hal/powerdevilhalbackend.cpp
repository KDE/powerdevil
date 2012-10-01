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
#include <Solid/DeviceNotifier>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnectionInterface>
#include <KDebug>
#include "halsuspendjob.h"
#include <Solid/Device>
#include <Solid/Button>
#include <Solid/Battery>
#include <Solid/GenericInterface>
#include <Solid/AcAdapter>
#include <KPluginFactory>
#include <QtCore/QTimer>

PowerDevilHALBackend::PowerDevilHALBackend(QObject* parent)
    : BackendInterface(parent),
      m_brightnessInHardware(false),
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
    qDeleteAll(m_acAdapters);
    qDeleteAll(m_batteries);
    qDeleteAll(m_buttons);
}

bool PowerDevilHALBackend::isAvailable()
{
    return QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.Hal");
}

void PowerDevilHALBackend::init()
{
    setCapabilities(NoCapabilities);

    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(QString)),
            this, SLOT(slotDeviceRemoved(QString)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(QString)),
            this, SLOT(slotDeviceAdded(QString)));

    m_pluggedAdapterCount = 0;
    computeAcAdapters();

    computeBatteries();
    updateBatteryStats();

    computeButtons();

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

    QList<QString> screenControls = controls.keys(Screen);

    if (!screenControls.isEmpty()) {
        m_cachedBrightness = brightness(Screen);

        QDBusInterface deviceInterface("org.freedesktop.Hal",
                                       screenControls.at(0),
                                       "org.freedesktop.Hal.Device",
                                       QDBusConnection::systemBus());
        QDBusReply<bool> replyInHardware = deviceInterface.call("GetPropertyBoolean",
                                                                "laptop_panel.brightness_in_hardware");
        if (replyInHardware.isValid()) {
            m_brightnessInHardware = replyInHardware;
        }
    }

    // Supported suspend methods
    SuspendMethods supported = UnknownSuspendMethod;
    {
        QDBusPendingReply<bool> reply = m_halComputer.asyncCall("GetPropertyBoolean", "power_management.can_suspend");
        reply.waitForFinished();

        if (reply.isValid()) {
            bool can_suspend = reply;
            if (can_suspend) {
                supported |= ToRam;
            }
        } else {
            kDebug() << reply.error().name() << ": " << reply.error().message();
        }

        reply = m_halComputer.asyncCall("GetPropertyBoolean", "power_management.can_hibernate");
        reply.waitForFinished();

        if (reply.isValid()) {
            bool can_hibernate = reply;
            if (can_hibernate) {
                supported |= ToDisk;
            }
        } else {
            kDebug() << reply.error().name() << ": " << reply.error().message();
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

    setBackendIsReady(controls, supported);
}

void PowerDevilHALBackend::brightnessKeyPressed(PowerDevil::BackendInterface::BrightnessKeyType type)
{
    BrightnessControlsList controls = brightnessControlsAvailable();
    QList<QString> screenControls = controls.keys(Screen);

    if (screenControls.isEmpty()) {
        return; // ignore as we are not able to determine the brightness level
    }

    float currentBrightness = brightness(Screen);

    if (qFuzzyCompare(currentBrightness, m_cachedBrightness) && !m_brightnessInHardware) {
        float newBrightness;
        if (type == Increase) {
            newBrightness = qMin(100.0f, currentBrightness + 10);
        } else {
            newBrightness = qMax(0.0f, currentBrightness - 10);
        }

        if (setBrightness(newBrightness, Screen)) {
            newBrightness = brightness(Screen);
            if (!qFuzzyCompare(newBrightness, m_cachedBrightness)) {
                m_cachedBrightness = newBrightness;
                onBrightnessChanged(Screen, m_cachedBrightness);
            }
        }
    } else {
        m_cachedBrightness = currentBrightness;
    }
}

float PowerDevilHALBackend::brightness(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    float brightness;
    if (type == Screen) {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                               "org.freedesktop.Hal.Device.LaptopPanel", QDBusConnection::systemBus());
                QDBusReply<int> brightnessReply = deviceInterface.call("GetBrightness");
                if(!brightnessReply.isValid()) {
                    return 0.0;
                }
                brightness = brightnessReply;

                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                 "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                QDBusReply<int> levelsReply = propertyInterface.call("GetProperty", "laptop_panel.num_levels");
                if (levelsReply.isValid()) {
                    int levels = levelsReply;
                    return (float)(100*(brightness/(levels-1)));
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
                if (!brightnessReply.isValid()) {
                    return 0.0;
                }
                brightness = brightnessReply;

                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                 "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                QDBusReply<int> levelsReply = propertyInterface.call("GetProperty", "keyboard_backlight.num_levels");
                if (levelsReply.isValid()) {
                    int levels = levelsReply;
                    return (float)(100*(brightness/(levels-1)));
                }
            }
        }
    }
    return 0.0;
}

bool PowerDevilHALBackend::setBrightness(float brightnessValue, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "laptop_panel");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                int levels = propertyInterface.call("GetProperty", "laptop_panel.num_levels").arguments().at(0).toInt();
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                            "org.freedesktop.Hal.Device.LaptopPanel", QDBusConnection::systemBus());
                deviceInterface.call("SetBrightness", qRound((levels-1)*(brightnessValue/100.0))); // .0? The right way? Feels hackish.
                if (!deviceInterface.lastError().isValid()) {
                    return true;
                }
            }
        }
    } else {
        QDBusPendingReply<QStringList> reply = m_halManager.asyncCall("FindDeviceByCapability", "keyboard_backlight");
        reply.waitForFinished();
        if(reply.isValid()) {
            foreach (const QString &device, reply.value()) {
                QDBusInterface propertyInterface("org.freedesktop.Hal", device,
                                                 "org.freedesktop.Hal.Device", QDBusConnection::systemBus());
                int levels = propertyInterface.call("GetProperty", "keyboard_backlight.num_levels").arguments().at(0).toInt();
                //TODO - I do not have a backlight enabled keyboard, so I'm guessing a bit here. Could someone please check this.
                QDBusInterface deviceInterface("org.freedesktop.Hal", device,
                                               "org.freedesktop.Hal.Device.KeyboardBacklight", QDBusConnection::systemBus());
                
                deviceInterface.call("SetBrightness", qRound((levels-1)*(brightnessValue/100.0)));
                if(!deviceInterface.lastError().isValid()) {
                    return true;
                }
            }
        }
    }
    return false;
}

KJob* PowerDevilHALBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    // Ok, that's not cool, but it's all HAL really gives us, so.
    QTimer::singleShot(0, this, SLOT(setResumeFromSuspend()));
    return new HalSuspendJob(m_halPowerManagement, m_halComputer,
                             method, supportedSuspendMethods());
}

void PowerDevilHALBackend::computeAcAdapters()
{
    QList<Solid::Device> adapters
        = Solid::Device::listFromType(Solid::DeviceInterface::AcAdapter);

    foreach (const Solid::Device &adapter, adapters) {
        m_acAdapters[adapter.udi()] = new Solid::Device(adapter);
        connect(m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>(), SIGNAL(plugStateChanged(bool,QString)),
                 this, SLOT(slotPlugStateChanged(bool)));

        if (m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>()!=0
            && m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>()->isPlugged()) {
            m_pluggedAdapterCount++;
        }
    }

    if (m_pluggedAdapterCount > 0) {
        setAcAdapterState(Plugged);
    } else {
        setAcAdapterState(Unplugged);
    }
}

void PowerDevilHALBackend::computeBatteries()
{
    QList<Solid::Device> batteries
        = Solid::Device::listFromQuery("Battery.type == 'PrimaryBattery'");

    foreach (const Solid::Device &battery, batteries) {
        m_batteries[battery.udi()] = new Solid::Device(battery);
        connect(m_batteries[battery.udi()]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(updateBatteryStats()));
        connect(m_batteries[battery.udi()]->as<Solid::GenericInterface>(), SIGNAL(propertyChanged(QMap<QString,int>)),
                 this, SLOT(slotBatteryPropertyChanged(QMap<QString,int>)));
    }

    updateBatteryStats();
}

void PowerDevilHALBackend::computeButtons()
{
    QList<Solid::Device> buttons
        = Solid::Device::listFromType(Solid::DeviceInterface::Button);

    foreach (const Solid::Device &button, buttons) {
        m_buttons[button.udi()] = new Solid::Device(button);
        connect(m_buttons[button.udi()]->as<Solid::Button>(), SIGNAL(pressed(Solid::Button::ButtonType,QString)),
                 this, SLOT(slotButtonPressed(Solid::Button::ButtonType)));
    }
}

void PowerDevilHALBackend::slotPlugStateChanged(bool newState)
{
    if (newState) {
        if(m_pluggedAdapterCount == 0) {
            setAcAdapterState(Plugged);
        }
        m_pluggedAdapterCount++;
    } else {
        if(m_pluggedAdapterCount == 1) {
            setAcAdapterState(Unplugged);
        }
        m_pluggedAdapterCount--;
    }
}

void PowerDevilHALBackend::slotButtonPressed(Solid::Button::ButtonType type)
{
    Solid::Button *button = qobject_cast<Solid::Button *>(sender());

    if (button == 0) return;

    switch(type) {
    case Solid::Button::PowerButton:
        setButtonPressed(PowerButton);
        break;
    case Solid::Button::SleepButton:
        setButtonPressed(SleepButton);
        break;
    case Solid::Button::LidButton:
        if (button->stateValue()) {
            setButtonPressed(LidClose);
        } else {
            setButtonPressed(LidOpen);
        }
        break;
    default:
        //kWarning() << "Unknown button type";
        break;
    }
}

void PowerDevilHALBackend::slotDeviceAdded(const QString &udi)
{
    Solid::Device *device = new Solid::Device(udi);
    if (device->is<Solid::AcAdapter>()) {
        m_acAdapters[udi] = device;
        connect(m_acAdapters[udi]->as<Solid::AcAdapter>(), SIGNAL(plugStateChanged(bool,QString)),
                 this, SLOT(slotPlugStateChanged(bool)));

        if (m_acAdapters[udi]->as<Solid::AcAdapter>()!=0
          && m_acAdapters[udi]->as<Solid::AcAdapter>()->isPlugged()) {
            m_pluggedAdapterCount++;
        }
    } else if (device->is<Solid::Battery>()) {
        m_batteries[udi] = device;
        connect(m_batteries[udi]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int,QString)),
                 this, SLOT(updateBatteryStats()));
        connect(m_batteries[udi]->as<Solid::GenericInterface>(), SIGNAL(propertyChanged(QMap<QString,int>)),
                 this, SLOT(slotBatteryPropertyChanged(QMap<QString,int>)));
    } else if (device->is<Solid::Button>()) {
        m_buttons[udi] = device;
        connect(m_buttons[udi]->as<Solid::Button>(), SIGNAL(pressed(int,QString)),
                 this, SLOT(slotButtonPressed(int)));
    } else {
        delete device;
    }
}

void PowerDevilHALBackend::slotDeviceRemoved(const QString &udi)
{
    Solid::Device *device = 0;

    device = m_acAdapters.take(udi);

    if (device != 0) {
        delete device;

        m_pluggedAdapterCount = 0;

        foreach (Solid::Device *d, m_acAdapters) {
            if (d->as<Solid::AcAdapter>()!=0
              && d->as<Solid::AcAdapter>()->isPlugged()) {
                m_pluggedAdapterCount++;
            }
        }

        return;
    }

    device = m_batteries.take(udi);

    if (device!=0) {
        delete device;
        updateBatteryStats();
        return;
    }

    device = m_buttons.take(udi);

    if (device!=0) {
        delete device;
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

#include "powerdevilhalbackend.moc"
