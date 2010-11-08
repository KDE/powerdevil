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

#include "powerdevilupowerbackend.h"

#include <qtextstream.h>
#include <QtDBus>

#include <KDebug>
#include <KPluginFactory>

#include "xrandrbrightness.h"
#include "upowersuspendjob.h"

K_PLUGIN_FACTORY(PowerDevilUpowerBackendFactory, registerPlugin<PowerDevilUPowerBackend>(); )
K_EXPORT_PLUGIN(PowerDevilUpowerBackendFactory("powerdevilupowerbackend"))

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent, const QVariantList&)
    : BackendInterface(parent),
      m_brightNessControl(new XRandrBrightness()),
      m_upowerInterface(new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), parent)),
      m_kbdBacklight(new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), parent)),
      m_lidIsPresent(false), m_lidIsClosed(false), m_onBattery(false)
{
}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend()
{
    qDeleteAll(m_devices);
    delete m_upowerInterface;
    delete m_brightNessControl;
    delete m_kbdBacklight;
}

void PowerDevilUPowerBackend::init()
{
    // devices
    enumerateDevices();
    connect(m_upowerInterface, SIGNAL(Changed()), this, SLOT(slotPropertyChanged()));
    connect(m_upowerInterface, SIGNAL(DeviceAdded(const QString &)), this, SLOT(slotDeviceAdded(const QString &)));
    connect(m_upowerInterface, SIGNAL(DeviceRemoved(const QString &)), this, SLOT(slotDeviceRemoved(const QString &)));
    connect(m_upowerInterface, SIGNAL(DeviceChanged(const QString &)), this, SLOT(slotDeviceChanged(const QString &)));

    // Brightness Controls available
    BrightnessControlsList controls;
    if (m_brightNessControl->isSupported()) {
        controls.insert(QLatin1String("LVDS1"), Screen);
    }

    if (m_kbdBacklight->isValid())
        controls.insert(QLatin1String("KBD"), Keyboard);

    if (!controls.isEmpty()) {
        m_cachedBrightness = brightness(Screen);
        kDebug() << "current screen brightness: " << m_cachedBrightness;
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
        kDebug() << "Screen brightness: " << m_brightNessControl->brightness();
        return m_brightNessControl->brightness();
    } else if (type == Keyboard) {
        kDebug() << "Kbd backlight: " << m_brightNessControl->brightness();
        return m_kbdBacklight->GetBrightness() / m_kbdBacklight->GetMaxBrightness() * 100;
    }

    return 0.0;
}

bool PowerDevilUPowerBackend::setBrightness(float brightnessValue, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        kDebug() << "set screen brightness: " << brightnessValue;
        m_brightNessControl->setBrightness(brightnessValue);
        float newBrightness = brightness(Screen);
        if (!qFuzzyCompare(newBrightness, m_cachedBrightness)) {
            m_cachedBrightness = newBrightness;
            onBrightnessChanged(Screen, m_cachedBrightness);
        }
        return true;
    } else if (type == Keyboard) {
        kDebug() << "set kbd backlight: " << brightnessValue;
        m_kbdBacklight->SetBrightness(brightnessValue / 100 * m_kbdBacklight->GetMaxBrightness());
        return true;
    }

    return false;
}

KJob* PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    return new UPowerSuspendJob(m_upowerInterface, method, supportedSuspendMethods());
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    m_lidIsClosed = m_upowerInterface->lidIsClosed();
    m_onBattery = m_upowerInterface->onBattery();

    QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
    foreach (const QDBusObjectPath & device, deviceList)
        slotDeviceAdded(device.path());

    if (m_onBattery)
        setAcAdapterState(Unplugged);
    else
        setAcAdapterState(Plugged);
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QString & device)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice =
            new OrgFreedesktopUPowerDeviceInterface(UPOWER_SERVICE, device, QDBusConnection::systemBus(), this);
    m_devices.insert(device, upowerDevice);

    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QString & device)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice = m_devices.take(device);

    if (upowerDevice)
        delete upowerDevice;

    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceChanged(const QString & /*device*/)
{
    updateDeviceProps();
}

void PowerDevilUPowerBackend::updateDeviceProps()
{
    qlonglong remainingTime = 0;

    foreach(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
        if (upowerDevice->type() == 2 && upowerDevice->powerSupply()) {
            if (upowerDevice->state() == 1) // charging
                remainingTime += upowerDevice->timeToFull();
            else if (upowerDevice->state() == 2) //discharging
                remainingTime += upowerDevice->timeToEmpty();
        }
    }

    setBatteryRemainingTime(remainingTime * 1000);
}

void PowerDevilUPowerBackend::slotPropertyChanged()
{
    // check for lid button changes
    if (m_lidIsPresent) {
        bool lidIsClosed = m_upowerInterface->lidIsClosed();
        if (lidIsClosed != m_lidIsClosed) {
            if (lidIsClosed)
                emit buttonPressed(LidClose);
            else
                emit buttonPressed(LidOpen);
        }
        m_lidIsClosed = lidIsClosed;
    }

    // check for AC adapter changes
    bool onBattery = m_upowerInterface->onBattery();
    if (m_onBattery != onBattery) {
        if (onBattery)
            setAcAdapterState(Unplugged);
        else
            setAcAdapterState(Plugged);
    }

    m_onBattery = onBattery;
}

#include "powerdevilupowerbackend.moc"
