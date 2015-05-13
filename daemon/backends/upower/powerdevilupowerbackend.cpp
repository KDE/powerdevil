/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2010-2015 Lukáš Tinkl <ltinkl@redhat.com>
    Copyright (C) 2015 Kai Uwe Broulik <kde@privat.broulik.de>

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

#include <powerdevil_debug.h>
#include <PowerDevilSettings.h>

#include <qtextstream.h>
#include <QtDBus>
#include <QDebug>
#include <QPropertyAnimation>

#include <kauthexecutejob.h>
#include <KAuthAction>

#include "xrandrxcbhelper.h"
#include "xrandrbrightness.h"
#include "udevqt.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent)
    : BackendInterface(parent),
      m_displayDevice(Q_NULLPTR),
      m_brightnessControl(Q_NULLPTR),
      m_kbdMaxBrightness(0)
{
}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend()
{
    delete m_brightnessControl;
}

bool PowerDevilUPowerBackend::isAvailable()
{
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
        // Is it pending activation?
        qCDebug(POWERDEVIL) << "UPower service, " << UPOWER_SERVICE << ", is not registered on the bus. Trying to find out if it is activated.";
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DBus",
                                                              "/org/freedesktop/DBus",
                                                              "org.freedesktop.DBus",
                                                              "ListActivatableNames");

        QDBusPendingReply< QStringList > reply = QDBusConnection::systemBus().asyncCall(message);
        reply.waitForFinished();

        if (reply.isValid()) {
            if (reply.value().contains(UPOWER_SERVICE)) {
                qCDebug(POWERDEVIL) << "UPower was found, activating service...";
                QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
                if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
                    // Wait for it
                    QEventLoop e;
                    QTimer *timer = new QTimer;
                    timer->setInterval(10000);
                    timer->setSingleShot(true);

                    connect(QDBusConnection::systemBus().interface(), SIGNAL(serviceRegistered(QString)),
                            &e, SLOT(quit()));
                    connect(timer, SIGNAL(timeout()), &e, SLOT(quit()));

                    timer->start();

                    while (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
                        e.exec();

                        if (!timer->isActive()) {
                            qCDebug(POWERDEVIL) << "Activation of UPower timed out. There is likely a problem with your configuration.";
                            timer->deleteLater();
                            return false;
                        }
                    }

                    timer->deleteLater();
                }
                return true;
            } else {
                qCDebug(POWERDEVIL) << "UPower cannot be found on this system.";
                return false;
            }
        } else {
            qCWarning(POWERDEVIL) << "Could not request activatable names to DBus!";
            return false;
        }
    } else {
        return true;
    }
}

void PowerDevilUPowerBackend::init()
{
    // interfaces
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
    }

    bool screenBrightnessAvailable = false;
    m_upowerInterface = new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);
    m_brightnessControl = new XRandrBrightness();
    if (!m_brightnessControl->isSupported()) {
        qCDebug(POWERDEVIL) << "Falling back to helper to get brightness";

        KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
        brightnessAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
        if (!brightnessJob->exec()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
        } else {
            m_cachedBrightnessMap.insert(Screen, brightnessJob->data()["brightness"].toFloat());

            KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
            brightnessMaxAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
            if (brightnessMaxJob->exec()) {
                m_brightnessMax = brightnessMaxJob->data()["brightnessmax"].toInt();
            } else {
                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
            }

            KAuth::Action syspathAction("org.kde.powerdevil.backlighthelper.syspath");
            syspathAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob* syspathJob = syspathAction.execute();
            if (syspathJob->exec()) {
                m_syspath = syspathJob->data()["syspath"].toString();
                m_syspath = QFileInfo(m_syspath).readLink();

                UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
                connect(client, SIGNAL(deviceChanged(UdevQt::Device)), SLOT(onDeviceChanged(UdevQt::Device)));

                screenBrightnessAvailable = true;
            }
        }
    } else {
        qCDebug(POWERDEVIL) << "Using XRandR";
        m_randrHelper = XRandRXCBHelper::self();
        Q_ASSERT(m_randrHelper);
        connect(m_randrHelper, &XRandRXCBHelper::brightnessChanged, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
        m_cachedBrightnessMap.insert(Screen, brightness(Screen));
        screenBrightnessAvailable = true;

        const int duration = PowerDevilSettings::brightnessAnimationDuration();
        if (duration > 0 && brightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
            m_brightnessAnimation = new QPropertyAnimation(this);
            m_brightnessAnimation->setTargetObject(this);
            m_brightnessAnimation->setDuration(duration);
            m_brightnessAnimation->setEasingCurve(QEasingCurve::InOutQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
        }
    }

    // Capabilities
    setCapabilities(SignalResumeFromSuspend);

    // devices
    enumerateDevices();

    connect(m_upowerInterface, SIGNAL(DeviceAdded(QString)), this, SLOT(slotDeviceAdded(QString)));
    connect(m_upowerInterface, SIGNAL(DeviceRemoved(QString)), this, SLOT(slotDeviceRemoved(QString)));
    // for UPower >= 0.99.0, changed signature :o/
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceAdded",
                                         this, SLOT(slotDeviceAdded(QDBusObjectPath)));
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceRemoved",
                                         this, SLOT(slotDeviceRemoved(QDBusObjectPath)));

    connect(m_upowerInterface, SIGNAL(DeviceChanged(QString)), this, SLOT(slotDeviceChanged(QString)));
    // for UPower >= 0.99.0, see slotDeviceAdded(const QString & device)

    // Brightness Controls available
    BrightnessControlsList controls;
    if (screenBrightnessAvailable) {
        controls.insert(QLatin1String("LVDS1"), Screen);
        qCDebug(POWERDEVIL) << "current screen brightness value: " << m_cachedBrightnessMap.value(Screen);
    }

    m_kbdBacklight = new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), this);
    if (m_kbdBacklight->isValid()) {
        // Cache max value
        QDBusPendingReply<int> rep = m_kbdBacklight->GetMaxBrightness();
        rep.waitForFinished();
        if (rep.isValid()) {
            m_kbdMaxBrightness = rep.value();
        }
        // TODO Do a proper check if the kbd backlight dbus object exists. But that should work for now ..
        if (m_kbdMaxBrightness) {
            controls.insert(QLatin1String("KBD"), Keyboard);
            m_cachedBrightnessMap.insert(Keyboard, brightness(Keyboard));
            qCDebug(POWERDEVIL) << "current keyboard backlight brightness value: " << m_cachedBrightnessMap.value(Keyboard);
            connect(m_kbdBacklight, SIGNAL(BrightnessChanged(int)), this, SLOT(onKeyboardBrightnessChanged(int)));
        }
    }

    // battery
    foreach(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
        if (upowerDevice->type() == 2 && upowerDevice->powerSupply()) {
            QString udi = upowerDevice->path();
            setCapacityForBattery(udi, qRound(upowerDevice->capacity()));  // acknowledge capacity
        }
    }

    // backend ready
    setBackendIsReady(controls);
}

void PowerDevilUPowerBackend::onDeviceChanged(const UdevQt::Device &device)
{
    qCDebug(POWERDEVIL) << "Udev device changed" << m_syspath << device.sysfsPath();
    if (device.sysfsPath() != m_syspath) {
        return;
    }

    int maxBrightness = device.sysfsProperty("max_brightness").toInt();
    if (maxBrightness <= 0) {
        return;
    }
    int newBrightness = device.sysfsProperty("brightness").toInt();

    if (newBrightness != m_cachedBrightnessMap[Screen]) {
        m_cachedBrightnessMap[Screen] = newBrightness;
        onBrightnessChanged(Screen, newBrightness, maxBrightness);
    }
}

int PowerDevilUPowerBackend::brightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type, BrightnessControlType controlType)
{
    const QStringList controls = brightnessControlsAvailable().keys(controlType);

    if (controls.isEmpty()) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = brightness(controlType);
    if (currentBrightness != m_cachedBrightnessMap.value(controlType)) {
        m_cachedBrightnessMap[controlType] = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = brightnessMax(controlType);
    int newBrightness = calculateNextStep(currentBrightness, maxBrightness, controlType, type);

    if (newBrightness < 0) {
        return -1;
    }

    setBrightness(newBrightness, controlType);
    return newBrightness;
}

int PowerDevilUPowerBackend::brightness(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    int result = 0;

    if (type == Screen) {
        if (m_brightnessControl->isSupported()) {
            if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
                result = m_brightnessAnimation->endValue().toInt();
            } else {
                //qCDebug(POWERDEVIL) << "Calling xrandr brightness";
                result = (int) m_brightnessControl->brightness();
            }
        } else {
            result = m_cachedBrightnessMap[Screen];
        }
        qCDebug(POWERDEVIL) << "Screen brightness value: " << result;
    } else if (type == Keyboard) {
        result = m_kbdBacklight->GetBrightness();
        qCDebug(POWERDEVIL) << "Kbd backlight brightness value: " << result;
    }

    return result;
}

int PowerDevilUPowerBackend::brightnessMax(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    int result = 0;

    if (type == Screen) {
        if (m_brightnessControl->isSupported()) {
            //qCDebug(POWERDEVIL) << "Calling xrandr brightness";
            result = (int) m_brightnessControl->brightnessMax();
        } else {
            result = m_brightnessMax;
        }
        qCDebug(POWERDEVIL) << "Screen brightness value max: " << result;
    } else if (type == Keyboard) {
        result = m_kbdMaxBrightness;
        qCDebug(POWERDEVIL) << "Kbd backlight brightness value max: " << result;
    }

    return result;
}

void PowerDevilUPowerBackend::setBrightness(int value, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == Screen) {
        qCDebug(POWERDEVIL) << "set screen brightness value: " << value;
        if (m_brightnessControl->isSupported()) {
            if (m_brightnessAnimation) {
                m_brightnessAnimation->stop();
                m_brightnessAnimation->setStartValue(brightness());
                m_brightnessAnimation->setEndValue(value);
                m_brightnessAnimation->start();
            } else {
                m_brightnessControl->setBrightness(value);
            }
        } else {
            //qCDebug(POWERDEVIL) << "Falling back to helper to set brightness";
            KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
            action.setHelperId(HELPER_ID);
            action.addArgument("brightness", value);
            KAuth::ExecuteJob *job = action.execute();
            // we don't care about the result since executing the job sync is bad
            job->start();
        }
    } else if (type == Keyboard) {
        qCDebug(POWERDEVIL) << "set kbd backlight value: " << value;
        m_kbdBacklight->SetBrightness(value);
    }
}

void PowerDevilUPowerBackend::slotScreenBrightnessChanged()
{
    if (m_brightnessAnimation && m_brightnessAnimation->state() != QPropertyAnimation::Stopped) {
        return;
    }

    int value = brightness(Screen);
    qCDebug(POWERDEVIL) << "Brightness changed!!";
    if (value != m_cachedBrightnessMap[Screen]) {
        m_cachedBrightnessMap[Screen] = value;
        onBrightnessChanged(Screen, value, brightnessMax(Screen));
    }
}

void PowerDevilUPowerBackend::onKeyboardBrightnessChanged(int value)
{
    qCDebug(POWERDEVIL) << "Keyboard brightness changed!!";
    if (value != m_cachedBrightnessMap[Keyboard]) {
        m_cachedBrightnessMap[Keyboard] = value;
        onBrightnessChanged(Keyboard, value, brightnessMax(Keyboard));
    }
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    const QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
    foreach (const QDBusObjectPath & device, deviceList) {
        addDevice(device.path());
    }

    QDBusReply<QDBusObjectPath> reply = m_upowerInterface->call("GetDisplayDevice");
    if (reply.isValid()) {
        const QString path = reply.value().path();
        if (!path.isEmpty() && path != QStringLiteral("/")) {
            m_displayDevice = new OrgFreedesktopUPowerDeviceInterface(UPOWER_SERVICE, path, QDBusConnection::systemBus(), this);
        }
    }

    updateDeviceProps();
}

void PowerDevilUPowerBackend::addDevice(const QString & device)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice =
            new OrgFreedesktopUPowerDeviceInterface(UPOWER_SERVICE, device, QDBusConnection::systemBus(), this);
    m_devices.insert(device, upowerDevice);

    // for UPower >= 0.99.0 which doesn't emit the DeviceChanged(QString) signal
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, device, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                         SLOT(onDevicePropertiesChanged(QString,QVariantMap,QStringList)));
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QString & device)
{
    addDevice(device);
    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QString & device)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice = m_devices.take(device);

    delete upowerDevice;

    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QDBusObjectPath &path)
{
    slotDeviceAdded(path.path());
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QDBusObjectPath &path)
{
    slotDeviceRemoved(path.path());
}

void PowerDevilUPowerBackend::slotDeviceChanged(const QString & /*device*/)
{
    updateDeviceProps();
}

void PowerDevilUPowerBackend::updateDeviceProps()
{
    qulonglong remainingTime = 0;

    if (m_displayDevice && m_displayDevice->isPresent()) {
        const uint state = m_displayDevice->state();
        if (state == 1) // charging
            remainingTime = m_displayDevice->timeToFull();
        else if (state == 2) //discharging
            remainingTime = m_displayDevice->timeToEmpty();
    } else {
        double energyTotal = 0.0;
        double energyRateTotal = 0.0;
        double energyFullTotal = 0.0;
        uint stateTotal = 0;

        foreach(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
            const uint type = upowerDevice->type();
            if ((type == 2 || type == 3) && upowerDevice->powerSupply()) {
                const uint state = upowerDevice->state();
                energyFullTotal += upowerDevice->energyFull();
                energyTotal += upowerDevice->energy();
                energyRateTotal += upowerDevice->energyRate();

                if (state == 1) { // total is charging
                    stateTotal = 1;
                } else if (state == 2 && stateTotal != 1) { // total is discharging
                    stateTotal = 2;
                } else if (state == 4 && stateTotal != 0) { // total is fully-charged
                    stateTotal = 4;
                }

                if (state == 1) { // charging
                    remainingTime += upowerDevice->timeToFull();
                } else if (state == 2) { // discharging
                    remainingTime += upowerDevice->timeToEmpty();
                }
            }
        }

        if (energyRateTotal > 0) {
            if (stateTotal == 1) { // charging
                remainingTime = 3600 * ((energyFullTotal - energyTotal) / energyRateTotal);
            } else if (stateTotal == 2) { // discharging
                remainingTime = 3600 * (energyTotal / energyRateTotal);
            }
        }
    }

    setBatteryRemainingTime(remainingTime * 1000);
}

void PowerDevilUPowerBackend::onDevicePropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    Q_UNUSED(changedProps);
    Q_UNUSED(invalidatedProps);

    if (ifaceName == UPOWER_IFACE_DEVICE) {
        updateDeviceProps(); // TODO maybe process the properties separately?
    }
}

void PowerDevilUPowerBackend::animationValueChanged(const QVariant &value)
{
    m_brightnessControl->setBrightness(value.toInt());
}
