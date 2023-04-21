/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
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

#include <QTextStream>
#include <QDBusMessage>
#include <QDebug>
#include <QPropertyAnimation>
#include <QTimer>

#include <kauth_version.h>
#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <KSharedConfig>

#include "ddcutilbrightness.h"
#include "login1suspendjob.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent)
    : BackendInterface(parent)
    , m_displayDevice(nullptr)
    , m_upowerInterface(nullptr)
    , m_kbdBacklight(nullptr)
    , m_kbdMaxBrightness(0)
    , m_lidIsPresent(false)
    , m_lidIsClosed(false)
    , m_onBattery(false)
    , m_isLedBrightnessControl(false)
{

}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend() = default;

void PowerDevilUPowerBackend::init()
{
    // interfaces
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(LOGIN1_SERVICE);
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(CONSOLEKIT2_SERVICE);
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        m_login1Interface = new QDBusInterface(LOGIN1_SERVICE, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus(), this);
    }

    // if login1 isn't available, try using the same interface with ConsoleKit2
    if (!m_login1Interface && QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        m_login1Interface = new QDBusInterface(CONSOLEKIT2_SERVICE, "/org/freedesktop/ConsoleKit/Manager", "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus(), this);
    }

    connect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    m_upowerInterface = new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);
    qCDebug(POWERDEVIL)<<"Trying ddc, helper for brightness controls";
    m_ddcBrightnessControl = new DDCutilBrightness();
    m_ddcBrightnessControl->detect();
    if (!m_ddcBrightnessControl->isSupported()) {
        qCDebug(POWERDEVIL) << "Falling back to helper to get brightness";

        KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
        brightnessAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
        connect(brightnessJob, &KJob::result, this,
            [this, brightnessJob]  {
                if (brightnessJob->error()) {
                    qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
                    qCDebug(POWERDEVIL) << brightnessJob->errorText();
                    Q_EMIT brightnessSupportQueried(false);
                    return;
                }
                m_cachedBrightnessMap.insert(Screen, brightnessJob->data()["brightness"].toFloat());

                KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
                brightnessMaxAction.setHelperId(HELPER_ID);
                KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
                connect(brightnessMaxJob, &KJob::result, this,
                    [this, brightnessMaxJob] {
                        if (brightnessMaxJob->error()) {
                            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
                            qCDebug(POWERDEVIL) << brightnessMaxJob->errorText();
                        } else {
                            m_brightnessMax = brightnessMaxJob->data()["brightnessmax"].toInt();
                        }

#ifdef Q_OS_FREEBSD
                        // FreeBSD doesn't have the sysfs interface that the bits below expect;
                        // the sysfs calls always fail, leading to brightnessSupportQueried(false) emission.
                        // Skip that command and carry on with the information that we do have.
                        Q_EMIT brightnessSupportQueried(m_brightnessMax > 0);
#else
                        KAuth::Action syspathAction("org.kde.powerdevil.backlighthelper.syspath");
                        syspathAction.setHelperId(HELPER_ID);
                        KAuth::ExecuteJob* syspathJob = syspathAction.execute();
                        connect(syspathJob, &KJob::result, this,
                            [this, syspathJob] {
                                if (syspathJob->error()) {
                                    qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.syspath failed";
                                    qCDebug(POWERDEVIL) << syspathJob->errorText();
                                    Q_EMIT brightnessSupportQueried(false);
                                    return;
                                }
                                m_syspath = syspathJob->data()["syspath"].toString();
                                m_syspath = QFileInfo(m_syspath).symLinkTarget();

                                m_isLedBrightnessControl = m_syspath.contains(QLatin1String("/leds/"));
                                if (!m_isLedBrightnessControl) {
                                    UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
                                    connect(client, &UdevQt::Client::deviceChanged, this, &PowerDevilUPowerBackend::onDeviceChanged);
                                }

                                Q_EMIT brightnessSupportQueried(m_brightnessMax > 0);
                            }
                        );
                        syspathJob->start();
#endif
                    }
                );
                brightnessMaxJob->start();
            }
        );
        brightnessJob->start();
    } else {
        qCDebug(POWERDEVIL) << "Using DDCutillib";
        m_cachedBrightnessMap.insert(Screen, brightness(Screen));

        const int duration = PowerDevilSettings::brightnessAnimationDuration();
        if (duration > 0 && brightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
            m_brightnessAnimation = new QPropertyAnimation(this);
            m_brightnessAnimation->setTargetObject(this);
            m_brightnessAnimation->setDuration(duration);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
        }
        Q_EMIT brightnessSupportQueried(true);
    }
}

void PowerDevilUPowerBackend::initWithBrightness(bool screenBrightnessAvailable)
{
    disconnect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    // Capabilities
    setCapabilities(SignalResumeFromSuspend);

    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                         SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceAdded",
                                         this, SLOT(slotDeviceAdded(QDBusObjectPath)));
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceRemoved",
                                         this, SLOT(slotDeviceRemoved(QDBusObjectPath)));

    // devices
    enumerateDevices();

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
            connect(m_kbdBacklight, &OrgFreedesktopUPowerKbdBacklightInterface::BrightnessChanged, this, &PowerDevilUPowerBackend::onKeyboardBrightnessChanged);
        }
    }

    // Supported suspend methods
    SuspendMethods supported = UnknownSuspendMethod;
    if (m_login1Interface) {
        QDBusPendingReply<QString> canSuspend = m_login1Interface.data()->asyncCall("CanSuspend");
        canSuspend.waitForFinished();
        if (canSuspend.isValid() && (canSuspend.value() == QLatin1String("yes") || canSuspend.value() == QLatin1String("challenge")))
            supported |= ToRam;

        QDBusPendingReply<QString> canHibernate = m_login1Interface.data()->asyncCall("CanHibernate");
        canHibernate.waitForFinished();
        if (canHibernate.isValid() && (canHibernate.value() == QLatin1String("yes") || canHibernate.value() == QLatin1String("challenge")))
            supported |= ToDisk;

        QDBusPendingReply<QString> canHybridSleep = m_login1Interface.data()->asyncCall("CanHybridSleep");
        canHybridSleep.waitForFinished();
        if (canHybridSleep.isValid() && (canHybridSleep.value() == QLatin1String("yes") || canHybridSleep.value() == QLatin1String("challenge")))
            supported |= HybridSuspend;

        QDBusPendingReply<QString> canSuspendThenHibernate = m_login1Interface.data()->asyncCall("CanSuspendThenHibernate");
        canSuspendThenHibernate.waitForFinished();
        if (canSuspendThenHibernate.isValid() && (canSuspendThenHibernate.value() == QLatin1String("yes") || canSuspendThenHibernate.value() == QLatin1String("challenge")))
            supported |= SuspendThenHibernate;
    }

    // "resuming" signal
    if (m_login1Interface) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1PrepareForSleep(bool)));
    }

    // backend ready
    setBackendIsReady(controls, supported);
}

void PowerDevilUPowerBackend::onDeviceChanged(const UdevQt::Device &device)
{
    // If we're currently in the process of changing brightness, ignore any such events
    if (m_brightnessAnimationTimer && m_brightnessAnimationTimer->isActive()) {
        return;
    }

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
    BrightnessControlsList allControls = brightnessControlsAvailable();
    QList<QString> controls = allControls.keys(controlType);

    if (controls.isEmpty()) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = brightness(controlType);
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (!(controlType == Screen &&
          m_brightnessAnimation &&
          m_brightnessAnimation->state() == QPropertyAnimation::Running) &&
        currentBrightness != m_cachedBrightnessMap.value(controlType)) {
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
        if (m_ddcBrightnessControl->isSupported()) {
            if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
                result = m_brightnessAnimation->endValue().toInt();
            } else {
                result = (int)m_ddcBrightnessControl->brightness();
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
        if (m_ddcBrightnessControl->isSupported()) {
            result = (int)m_ddcBrightnessControl->brightnessMax();
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
        if (m_ddcBrightnessControl->isSupported()) {
            if (m_brightnessAnimation) {
                m_brightnessAnimation->stop();
                disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
                m_brightnessAnimation->setStartValue(brightness());
                m_brightnessAnimation->setEndValue(value);
                m_brightnessAnimation->setEasingCurve(brightness() < value ? QEasingCurve::OutQuad : QEasingCurve::InQuad);
                connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
                m_brightnessAnimation->start();
            } else {
                m_ddcBrightnessControl->setBrightness((long)value);
            }
        } else {
            KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
            action.setHelperId(HELPER_ID);
            action.addArgument("brightness", value);
            if (brightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
                action.addArgument("animationDuration", PowerDevilSettings::brightnessAnimationDuration());
            }
            auto *job = action.execute();
            connect(job, &KAuth::ExecuteJob::result, this, [this, job, value] {
                if (job->error()) {
                    qCWarning(POWERDEVIL) << "Failed to set screen brightness" << job->errorText();
                    return;
                }

                // Immediately announce the new brightness to everyone while we still animate to it
                m_cachedBrightnessMap[Screen] = value;
                onBrightnessChanged(Screen, value, brightnessMax(Screen));

                // So we ignore any brightness changes during the animation
                if (!m_brightnessAnimationTimer) {
                    m_brightnessAnimationTimer = new QTimer(this);
                    m_brightnessAnimationTimer->setSingleShot(true);
                }
                m_brightnessAnimationTimer->start(PowerDevilSettings::brightnessAnimationDuration());
            });
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

    if (m_brightnessAnimationTimer && m_brightnessAnimationTimer->isActive()) {
        return;
    }

    int value = brightness(Screen);
    if (value != m_cachedBrightnessMap[Screen] || m_isLedBrightnessControl) {
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

KJob* PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    if (m_login1Interface) {
        return new Login1SuspendJob(m_login1Interface.data(), method, supportedSuspendMethods());
    }
    return nullptr;
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    setLidPresent(m_lidIsPresent);
    m_lidIsClosed = m_upowerInterface->lidIsClosed();
    m_onBattery = m_upowerInterface->onBattery();

    const QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
    for (const QDBusObjectPath & device : deviceList) {
        if (m_devices.contains(device.path())) {
            continue;
        }
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

    if (m_onBattery)
        setAcAdapterState(Unplugged);
    else
        setAcAdapterState(Plugged);
}

void PowerDevilUPowerBackend::addDevice(const QString & device)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice =
            new OrgFreedesktopUPowerDeviceInterface(UPOWER_SERVICE, device, QDBusConnection::systemBus(), this);
    m_devices.insert(device, upowerDevice);

    QDBusConnection::systemBus().connect(UPOWER_SERVICE, device, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                         SLOT(onDevicePropertiesChanged(QString,QVariantMap,QStringList)));
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QDBusObjectPath &path)
{
    addDevice(path.path());
    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QDBusObjectPath &path)
{
    OrgFreedesktopUPowerDeviceInterface * upowerDevice = m_devices.take(path.path());

    delete upowerDevice;

    updateDeviceProps();
}

void PowerDevilUPowerBackend::updateDeviceProps()
{
    double energyTotal = 0.0;
    double energyRateTotal = 0.0;
    double energyFullTotal = 0.0;
    qulonglong timestamp = 0;

    if (m_displayDevice && m_displayDevice->isPresent()) {
        const uint state = m_displayDevice->state();
        energyTotal = m_displayDevice->energy();
        energyFullTotal = m_displayDevice->energyFull();
        timestamp = m_displayDevice->updateTime();

        if (state == 1) { // charging
            energyRateTotal = m_displayDevice->energyRate();
        } else if (state == 2) { //discharging
            energyRateTotal = -1.0 * m_displayDevice->energyRate();
        }
    } else {
        for (const OrgFreedesktopUPowerDeviceInterface * upowerDevice : qAsConst(m_devices)) {
            const uint type = upowerDevice->type();
            if (( type == 2 || type == 3) && upowerDevice->powerSupply()) {
                const uint state = upowerDevice->state();
                energyFullTotal += upowerDevice->energyFull();
                energyTotal += upowerDevice->energy();

                if (state == 4) { // fully charged
                    continue;
                }

                timestamp = std::max(timestamp, upowerDevice->updateTime());
                if (state == 1) { // charging
                    energyRateTotal += upowerDevice->energyRate();
                } else if (state == 2) { // discharging
                    energyRateTotal -= upowerDevice->energyRate();
                }
            }
        }
    }

    setBatteryEnergy(energyTotal);
    setBatteryEnergyFull(energyFullTotal);
    setBatteryRate(energyRateTotal, timestamp);
}

void PowerDevilUPowerBackend::onPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    if (ifaceName != UPOWER_IFACE) {
        return;
    }

    if (m_lidIsPresent) {
        bool lidIsClosed = m_lidIsClosed;
        if (changedProps.contains(QStringLiteral("LidIsClosed"))) {
            lidIsClosed = changedProps[QStringLiteral("LidIsClosed")].toBool();
        } else if (invalidatedProps.contains(QStringLiteral("LidIsClosed"))) {
            lidIsClosed = m_upowerInterface->lidIsClosed();
        }
        if (lidIsClosed != m_lidIsClosed) {
            setButtonPressed(lidIsClosed ? LidClose : LidOpen);
            m_lidIsClosed = lidIsClosed;
        }
    }

    bool onBattery = m_onBattery;
    if (changedProps.contains(QStringLiteral("OnBattery"))) {
        onBattery = changedProps[QStringLiteral("OnBattery")].toBool();
    } else if (invalidatedProps.contains(QStringLiteral("OnBattery"))) {
        onBattery = m_upowerInterface->onBattery();
    }
    if (onBattery != m_onBattery) {
        setAcAdapterState(onBattery ? Unplugged : Plugged);
        m_onBattery = onBattery;
    }
}

void PowerDevilUPowerBackend::onDevicePropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    Q_UNUSED(changedProps);
    Q_UNUSED(invalidatedProps);

    if (ifaceName == UPOWER_IFACE_DEVICE) {
        updateDeviceProps(); // TODO maybe process the properties separately?
    }
}

void PowerDevilUPowerBackend::slotLogin1PrepareForSleep(bool active)
{
    if (active) {
        Q_EMIT aboutToSuspend();
    } else {
        Q_EMIT resumeFromSuspend();
    }
}

void PowerDevilUPowerBackend::animationValueChanged(const QVariant &value)
{
    if (m_ddcBrightnessControl->isSupported()) {
        m_ddcBrightnessControl->setBrightness(value.toInt());
    } else {
        qCInfo(POWERDEVIL)<<"PowerDevilUPowerBackend::animationValueChanged: brightness control not supported";
    }
}
