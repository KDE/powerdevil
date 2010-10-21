/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2010 Lukas Tinkl <ltinkl@redhat.com>

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

#include <qtextstream.h>

#include "upower_interface.h"
#include "powerdevilupowerbackend.h"
#include "xrandrbrightness.h"
#include "upowersuspendjob.h"

#include <QtDBus/QDBusReply>

#include <KDebug>

#include <Solid/Device>
#include <Solid/Button>
#include <Solid/Battery>
#include <Solid/GenericInterface>
#include <Solid/AcAdapter>
#include <Solid/DeviceNotifier>
#include <KPluginFactory>

K_PLUGIN_FACTORY(PowerDevilUpowerBackendFactory, registerPlugin<PowerDevilUPowerBackend>(); )
K_EXPORT_PLUGIN(PowerDevilUpowerBackendFactory("powerdevilupowerbackend"))

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent, const QVariantList&)
    : BackendInterface(parent),
      m_brightNessControl(new XRandrBrightness()),
      m_upowerInterface(new OrgFreedesktopUPowerInterface(
              "org.freedesktop.UPower",
              "/org/freedesktop/UPower",
              QDBusConnection::systemBus()))
{
}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend()
{
    qDeleteAll(m_acAdapters);
    qDeleteAll(m_batteries);
    qDeleteAll(m_buttons);
    delete m_upowerInterface;
    delete m_brightNessControl;
}

void PowerDevilUPowerBackend::init()
{
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)),
            this, SLOT(slotDeviceRemoved(const QString &)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)),
            this, SLOT(slotDeviceAdded(const QString &)));

    m_pluggedAdapterCount = 0;
    computeAcAdapters();

    computeBatteries();
    updateBatteryStats();

    computeButtons();

    // Brightness Control available
    BrightnessControlsList controls;
    {
        if (m_brightNessControl->isSupported()) {
            controls.insert(QLatin1String("LVDS1"), Screen);
        }

        //TODO: UPower will support kbd backlight in the next version, add it to controls
    }

    if (!controls.isEmpty()) {
        m_cachedBrightness = brightness(Screen);
        kDebug() << "current brightness: " << m_cachedBrightness;
    }

    // Supported suspend methods
    SuspendMethods supported = UnknownSuspendMethod;
    {
        if (m_upowerInterface->canSuspend()) {
            kDebug() << "Can suspend";
            supported |= ToRam;
        }

        if (m_upowerInterface->canHibernate()) {
            kDebug() << "Can hibernate";
            supported |= ToDisk;
        }
    }

    connect(m_upowerInterface, SIGNAL(Resuming()), this, SIGNAL(resumeFromSuspend()));

    setBackendIsReady(controls, supported);
}

void PowerDevilUPowerBackend::brightnessKeyPressed(PowerDevil::BackendInterface::BrightnessKeyType type)
{
    BrightnessControlsList controls = brightnessControlsAvailable();
    QList<QString> screenControls = controls.keys(Screen);

    if (screenControls.isEmpty()) {
        return; // ignore as we are not able to determine the brightness level
    }

    float currentBrightness = brightness(Screen);

    if (qFuzzyCompare(currentBrightness, m_cachedBrightness)) {
        float newBrightness;
        if (type == Increase) {
            newBrightness = qMin(100.0f, currentBrightness + 10);
        } else {
            newBrightness = qMax(0.0f, currentBrightness - 10);
        }

        setBrightness(newBrightness, Screen);
    } else {
        m_cachedBrightness = currentBrightness;
    }
}

float PowerDevilUPowerBackend::brightness(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    if (type == Screen) {
        qDebug() << "Brightness: " << m_brightNessControl->brightness();
        return m_brightNessControl->brightness();
    } else {
        //TODO: UPower will support kbd backlight in the next version
    }
    return 0.0;
}

bool PowerDevilUPowerBackend::setBrightness(float brightnessValue, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        qDebug() << "setBrightness: " << brightnessValue;
        m_brightNessControl->setBrightness(brightnessValue);
    } else {
        //TODO: UPower will support kbd backlight in the next version
    }
    return false;
}

KJob* PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    return new UPowerSuspendJob(m_upowerInterface, method, supportedSuspendMethods());
}

void PowerDevilUPowerBackend::computeAcAdapters()
{
    QList<Solid::Device> adapters = Solid::Device::listFromType(Solid::DeviceInterface::AcAdapter);

    foreach (const Solid::Device &adapter, adapters) {
        m_acAdapters[adapter.udi()] = new Solid::Device(adapter);
        connect(m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>(), SIGNAL(plugStateChanged(bool, const QString &)),
                this, SLOT(slotPlugStateChanged(bool)));

        if (m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>()!=0
                && m_acAdapters[adapter.udi()]->as<Solid::AcAdapter>()->isPlugged()) {
            m_pluggedAdapterCount++;
        }
    }
}

void PowerDevilUPowerBackend::computeBatteries()
{
    QList<Solid::Device> batteries = Solid::Device::listFromQuery("Battery.type == 'PrimaryBattery'");

    foreach (const Solid::Device &battery, batteries) {
        m_batteries[battery.udi()] = new Solid::Device(battery);
        connect(m_batteries[battery.udi()]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int, const QString &)),
                this, SLOT(slotBatteryChargeChanged()));
    }
}

void PowerDevilUPowerBackend::computeButtons()
{
    QList<Solid::Device> buttons = Solid::Device::listFromType(Solid::DeviceInterface::Button);

    foreach (const Solid::Device &button, buttons) {
        m_buttons[button.udi()] = new Solid::Device(button);
        connect(m_buttons[button.udi()]->as<Solid::Button>(), SIGNAL(pressed(Solid::Button::ButtonType, const QString &)),
                this, SLOT(slotButtonPressed(Solid::Button::ButtonType)));
    }
}

void PowerDevilUPowerBackend::slotPlugStateChanged(bool newState)
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

void PowerDevilUPowerBackend::slotButtonPressed(Solid::Button::ButtonType type)
{
    Solid::Button *button = qobject_cast<Solid::Button *>(sender());

    if (button == 0) return;

    switch(type) {
    case Solid::Button::PowerButton:
        emit buttonPressed(PowerButton);
        break;
    case Solid::Button::SleepButton:
        emit buttonPressed(SleepButton);
        break;
    case Solid::Button::LidButton:
        if (button->stateValue()) {
            emit buttonPressed(LidClose);
        } else {
            emit buttonPressed(LidOpen);
        }
        break;
    default:
        //kWarning() << "Unknown button type";
        break;
    }
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QString &udi)
{
    Solid::Device *device = new Solid::Device(udi);
    if (device->is<Solid::AcAdapter>()) {
        m_acAdapters[udi] = device;
        connect(m_acAdapters[udi]->as<Solid::AcAdapter>(), SIGNAL(plugStateChanged(bool, const QString &)),
                this, SLOT(slotPlugStateChanged(bool)));

        if (m_acAdapters[udi]->as<Solid::AcAdapter>()!=0 && m_acAdapters[udi]->as<Solid::AcAdapter>()->isPlugged()) {
            m_pluggedAdapterCount++;
        }
    } else if (device->is<Solid::Battery>()) {
        m_batteries[udi] = device;
        connect(m_batteries[udi]->as<Solid::Battery>(), SIGNAL(chargePercentChanged(int, const QString &)),
                this, SLOT(slotBatteryChargeChanged()));
    } else if (device->is<Solid::Button>()) {
        m_buttons[udi] = device;
        connect(m_buttons[udi]->as<Solid::Button>(), SIGNAL(pressed(int, const QString &)),
                this, SLOT(slotButtonPressed(int)));
    } else {
        delete device;
    }
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QString &udi)
{
    Solid::Device *device = 0;

    device = m_acAdapters.take(udi);

    if (device != 0) {
        delete device;

        m_pluggedAdapterCount = 0;

        foreach (Solid::Device *d, m_acAdapters) {
            if (d->as<Solid::AcAdapter>()!=0 && d->as<Solid::AcAdapter>()->isPlugged()) {
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

void PowerDevilUPowerBackend::slotBatteryChargeChanged()
{
    updateBatteryStats();
    setBatteryState(batteryState());
    setBatteryRemainingTime(m_estimatedBatteryTime);
}

void PowerDevilUPowerBackend::updateBatteryStats()
{
    m_currentBatteryCharge = 0;
    m_estimatedBatteryTime = 0;

    foreach (Solid::Device *d, m_batteries) {
        Solid::Battery *interface = d->as<Solid::Battery>();

        if (interface == 0) continue;

        m_currentBatteryCharge+= interface->chargePercent();
        m_estimatedBatteryTime+= interface->property("TimeToEmpty").toLongLong() * 1000;
    }
}

PowerDevil::BackendInterface::BatteryState PowerDevilUPowerBackend::batteryState() const
{
    if (m_batteries.isEmpty())
        return PowerDevil::BackendInterface::NoBatteryState;
    else if (m_currentBatteryCharge <= 5)
        return PowerDevil::BackendInterface::Critical;
    else if (m_currentBatteryCharge <= 10)
        return PowerDevil::BackendInterface::Low;
    else if (m_currentBatteryCharge <= 20)
        return PowerDevil::BackendInterface::Warning;
    else
        return PowerDevil::BackendInterface::Normal;
}

#include "powerdevilupowerbackend.moc"
