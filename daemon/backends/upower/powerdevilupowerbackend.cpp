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

#include <PowerDevilSettings.h>
#include <powerdevil_debug.h>

#include <QDBusMessage>
#include <QDebug>
#include <QPropertyAnimation>
#include <QTextStream>
#include <QTimer>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kauth_version.h>

#include "ddcutilbrightness.h"
#include "login1suspendjob.h"
#include "properties_interface.h"
#include "upowerdevice.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

inline constexpr QLatin1String LOGIN1_SESSION_INTERFACE{"org.freedesktop.login1.Session"};
inline constexpr QLatin1String LOGIN1_SESSION_AUTO_PATH{"/org/freedesktop/login1/session/auto"};

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject *parent)
    : BackendInterface(parent)
    , m_displayDevice(nullptr)
    , m_cachedScreenBrightness(0)
    , m_cachedKeyboardBrightness(0)
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
    // Bindings
    m_isSessionIdle.setBinding([this] {
        return m_login1SessionIdleHint.value() || m_backendIdleHint.value();
    });

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
        m_login1SessionPropInterface = new OrgFreedesktopDBusPropertiesInterface(LOGIN1_SERVICE, LOGIN1_SESSION_AUTO_PATH, QDBusConnection::systemBus(), this);
        QDBusPendingReply<QVariantMap> pending = m_login1SessionPropInterface->GetAll(LOGIN1_SESSION_INTERFACE);
        auto watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            QDBusReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (reply.isValid()) {
                onLogin1SessionPropertiesChanged({}, reply.value(), {});
            }
        });
    }

    // if login1 isn't available, try using the same interface with ConsoleKit2
    if (!m_login1Interface && QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        m_login1Interface = new QDBusInterface(CONSOLEKIT2_SERVICE,
                                               "/org/freedesktop/ConsoleKit/Manager",
                                               "org.freedesktop.ConsoleKit.Manager",
                                               QDBusConnection::systemBus(),
                                               this);
    }

    connect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    m_upowerInterface = new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);
    m_ddcBrightnessControl = new DDCutilBrightness();

    qCDebug(POWERDEVIL) << "Trying Backlight Helper first...";
    KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
    brightnessAction.setHelperId(HELPER_ID);
    KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
    connect(brightnessJob, &KJob::result, this, [this, brightnessJob] {
        if (brightnessJob->error()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
            qCDebug(POWERDEVIL) << brightnessJob->errorText();
            Q_EMIT brightnessSupportQueried(false);
            return;
        }
        m_cachedScreenBrightness = brightnessJob->data()["brightness"].toFloat();

        KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
        brightnessMaxAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
        connect(brightnessMaxJob, &KJob::result, this, [this, brightnessMaxJob] {
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
            connect(syspathJob, &KJob::result, this, [this, syspathJob] {
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
            });
            syspathJob->start();
#endif
        });
        brightnessMaxJob->start();
    });
    brightnessJob->start();
}

void PowerDevilUPowerBackend::initWithBrightness(bool screenBrightnessAvailable)
{
    if (!screenBrightnessAvailable) {
        qCDebug(POWERDEVIL) << "Brightness Helper have failed. Trying DDC Helper for brightness controls...";
        m_ddcBrightnessControl->detect();
        if (m_ddcBrightnessControl->isSupported()) {
            qCDebug(POWERDEVIL) << "Using DDCutillib";
            m_cachedScreenBrightness = screenBrightness();
            const int duration = PowerDevilSettings::brightnessAnimationDuration();
            if (duration > 0 && screenBrightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
                m_brightnessAnimation = new QPropertyAnimation(this);
                m_brightnessAnimation->setTargetObject(this);
                m_brightnessAnimation->setDuration(duration);
                connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
                connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
            }
            screenBrightnessAvailable = true;
        }
    }

    disconnect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    // Capabilities
    setCapabilities(SignalResumeFromSuspend);

    QDBusConnection::systemBus().connect(UPOWER_SERVICE,
                                         UPOWER_PATH,
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         this,
                                         SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceAdded", this, SLOT(slotDeviceAdded(QDBusObjectPath)));
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, UPOWER_IFACE, "DeviceRemoved", this, SLOT(slotDeviceRemoved(QDBusObjectPath)));

    // devices
    enumerateDevices();

    // Brightness Controls available
    if (screenBrightnessAvailable) {
        m_screenBrightnessAvailable = true;
        qCDebug(POWERDEVIL) << "current screen brightness value: " << m_cachedScreenBrightness;
    }

    m_kbdBacklight = new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), this);
    if (m_kbdBacklight->isValid()) {
        // Cache max value
        QDBusPendingReply<int> rep = m_kbdBacklight->GetMaxBrightness();
        rep.waitForFinished();
        if (rep.isValid()) {
            m_kbdMaxBrightness = rep.value();
            m_keyboardBrightnessAvailable = true;
        }
        // TODO Do a proper check if the kbd backlight dbus object exists. But that should work for now ..
        if (m_kbdMaxBrightness) {
            m_cachedKeyboardBrightness = keyboardBrightness();
            qCDebug(POWERDEVIL) << "current keyboard backlight brightness value: " << m_cachedKeyboardBrightness;
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
        if (canSuspendThenHibernate.isValid()
            && (canSuspendThenHibernate.value() == QLatin1String("yes") || canSuspendThenHibernate.value() == QLatin1String("challenge")))
            supported |= SuspendThenHibernate;
    }

    // "resuming" signal
    if (m_login1Interface) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1PrepareForSleep(bool)));
    }

    if (m_login1SessionPropInterface) {
        connect(m_login1SessionPropInterface,
                &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged,
                this,
                &PowerDevilUPowerBackend::onLogin1SessionPropertiesChanged);
    }

    // backend ready
    setBackendIsReady(supported);
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

    if (newBrightness != m_cachedScreenBrightness) {
        m_cachedScreenBrightness = newBrightness;
        onScreenBrightnessChanged(newBrightness, maxBrightness);
    }
}

int PowerDevilUPowerBackend::screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_screenBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = screenBrightness();
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (!(m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) && currentBrightness != m_cachedScreenBrightness) {
        m_cachedScreenBrightness = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = screenBrightnessMax();
    int newBrightness = calculateNextScreenBrightnessStep(currentBrightness, maxBrightness, type);

    if (newBrightness < 0) {
        return -1;
    }

    setScreenBrightness(newBrightness);
    return newBrightness;
}

int PowerDevilUPowerBackend::keyboardBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_keyboardBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = keyboardBrightness();
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (currentBrightness != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = keyboardBrightnessMax();
    int newBrightness = calculateNextKeyboardBrightnessStep(currentBrightness, maxBrightness, type);

    if (newBrightness < 0) {
        return -1;
    }

    setKeyboardBrightness(newBrightness);
    return newBrightness;
}

int PowerDevilUPowerBackend::screenBrightness() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
            result = m_brightnessAnimation->endValue().toInt();
        } else {
            result = m_ddcBrightnessControl->brightness(m_ddcBrightnessControl->displayIds().constFirst());
        }
    } else {
        result = m_cachedScreenBrightness;
    }
    qCDebug(POWERDEVIL) << "Screen brightness value: " << result;
    return result;
}

int PowerDevilUPowerBackend::screenBrightnessMax() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        result = m_ddcBrightnessControl->brightnessMax(m_ddcBrightnessControl->displayIds().constFirst());
    } else {
        result = m_brightnessMax;
    }
    qCDebug(POWERDEVIL) << "Screen brightness value max: " << result;

    return result;
}

int PowerDevilUPowerBackend::keyboardBrightnessMax() const
{
    int result = m_kbdMaxBrightness;
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value max: " << result;

    return result;
}

void PowerDevilUPowerBackend::setScreenBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set screen brightness value: " << value;
    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation) {
            m_brightnessAnimation->stop();
            disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->setStartValue(screenBrightness());
            m_brightnessAnimation->setEndValue(value);
            m_brightnessAnimation->setEasingCurve(screenBrightness() < value ? QEasingCurve::OutQuad : QEasingCurve::InQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->start();
        } else {
            m_ddcBrightnessControl->setBrightness(m_ddcBrightnessControl->displayIds().constFirst(), value);
        }
    } else {
        KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
        action.setHelperId(HELPER_ID);
        action.addArgument("brightness", value);
        if (screenBrightness() >= PowerDevilSettings::brightnessAnimationThreshold()) {
            action.addArgument("animationDuration", PowerDevilSettings::brightnessAnimationDuration());
        }
        auto *job = action.execute();
        connect(job, &KAuth::ExecuteJob::result, this, [this, job, value] {
            if (job->error()) {
                qCWarning(POWERDEVIL) << "Failed to set screen brightness" << job->errorText();
                return;
            }

            // Immediately announce the new brightness to everyone while we still animate to it
            m_cachedScreenBrightness = value;
            onScreenBrightnessChanged(value, screenBrightnessMax());

            // So we ignore any brightness changes during the animation
            if (!m_brightnessAnimationTimer) {
                m_brightnessAnimationTimer = new QTimer(this);
                m_brightnessAnimationTimer->setSingleShot(true);
            }
            m_brightnessAnimationTimer->start(PowerDevilSettings::brightnessAnimationDuration());
        });
        job->start();
    }
}

bool PowerDevilUPowerBackend::screenBrightnessAvailable() const
{
    return m_screenBrightnessAvailable;
}

void PowerDevilUPowerBackend::setKeyboardBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << value;
    m_kbdBacklight->SetBrightness(value);
}

bool PowerDevilUPowerBackend::keyboardBrightnessAvailable() const
{
    return m_keyboardBrightnessAvailable;
}

void PowerDevilUPowerBackend::slotScreenBrightnessChanged()
{
    if (m_brightnessAnimation && m_brightnessAnimation->state() != QPropertyAnimation::Stopped) {
        return;
    }

    if (m_brightnessAnimationTimer && m_brightnessAnimationTimer->isActive()) {
        return;
    }

    int value = screenBrightness();
    if (value != m_cachedScreenBrightness || m_isLedBrightnessControl) {
        m_cachedScreenBrightness = value;
        onScreenBrightnessChanged(value, screenBrightnessMax());
    }
}

void PowerDevilUPowerBackend::onKeyboardBrightnessChanged(int value)
{
    qCDebug(POWERDEVIL) << "Keyboard brightness changed!!";
    if (value != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = value;
        BackendInterface::onKeyboardBrightnessChanged(value, keyboardBrightnessMax());
    }
}

KJob *PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    if (m_login1Interface) {
        return new Login1SuspendJob(m_login1Interface.data(), method, supportedSuspendMethods());
    }
    return nullptr;
}

void PowerDevilUPowerBackend::setIdleHint(bool idle)
{
    m_backendIdleHint = idle;
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    setLidPresent(m_lidIsPresent);
    m_lidIsClosed = m_upowerInterface->lidIsClosed();
    m_onBattery = m_upowerInterface->onBattery();

    QDBusReply<QDBusObjectPath> reply = m_upowerInterface->call("GetDisplayDevice");
    if (reply.isValid()) {
        const QString path = reply.value().path();
        if (!path.isEmpty() && path != QStringLiteral("/")) {
            m_displayDevice = std::make_unique<UPowerDevice>(path);
            connect(m_displayDevice.get(), &UPowerDevice::propertiesChanged, this, &PowerDevilUPowerBackend::updateDeviceProps);
        }
    }

    if (!m_displayDevice) {
        const QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
        for (const QDBusObjectPath &device : deviceList) {
            if (m_devices.count(device.path())) {
                continue;
            }
            addDevice(device.path());
        }
    }

    updateDeviceProps();

    if (m_onBattery)
        setAcAdapterState(Unplugged);
    else
        setAcAdapterState(Plugged);
}

void PowerDevilUPowerBackend::addDevice(const QString &device)
{
    if (m_displayDevice) {
        return;
    }

    auto upowerDevice = std::make_unique<UPowerDevice>(device);
    connect(upowerDevice.get(), &UPowerDevice::propertiesChanged, this, &PowerDevilUPowerBackend::updateDeviceProps);

    m_devices[device] = std::move(upowerDevice);
}

void PowerDevilUPowerBackend::slotDeviceAdded(const QDBusObjectPath &path)
{
    addDevice(path.path());
    updateDeviceProps();
}

void PowerDevilUPowerBackend::slotDeviceRemoved(const QDBusObjectPath &path)
{
    m_devices.erase(path.path());
    updateDeviceProps();
}

void PowerDevilUPowerBackend::updateDeviceProps()
{
    double energyTotal = 0.0;
    double energyRateTotal = 0.0;
    double energyFullTotal = 0.0;
    qulonglong timestamp = 0;

    if (m_displayDevice) {
        if (!m_displayDevice->isPresent()) {
            // No Battery/Ups, nothing to report
            return;
        }
        const auto state = m_displayDevice->state();
        energyTotal = m_displayDevice->energy();
        energyFullTotal = m_displayDevice->energyFull();
        timestamp = m_displayDevice->updateTime();

        if (state == UPowerDevice::State::Charging) {
            energyRateTotal = m_displayDevice->energyRate();
        } else if (state == UPowerDevice::State::Discharging) {
            energyRateTotal = -1.0 * m_displayDevice->energyRate();
        }
    } else {
        for (const auto &[key, upowerDevice] : m_devices) {
            if (!upowerDevice->isPowerSupply()) {
                continue;
            }
            const auto type = upowerDevice->type();
            if (type == UPowerDevice::Type::Battery || type == UPowerDevice::Type::Ups) {
                const auto state = upowerDevice->state();
                energyFullTotal += upowerDevice->energyFull();
                energyTotal += upowerDevice->energy();

                if (state == UPowerDevice::State::FullyCharged) {
                    continue;
                }

                timestamp = std::max(timestamp, upowerDevice->updateTime());
                if (state == UPowerDevice::State::Charging) {
                    energyRateTotal += upowerDevice->energyRate();
                } else if (state == UPowerDevice::State::Discharging) {
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

void PowerDevilUPowerBackend::slotLogin1PrepareForSleep(bool active)
{
    if (active) {
        Q_EMIT aboutToSuspend();
    } else {
        Q_EMIT resumeFromSuspend();
    }
}

void PowerDevilUPowerBackend::onLogin1SessionPropertiesChanged(const QString &, const QVariantMap &changedProps, const QStringList &)
{
    auto it = changedProps.constFind(QStringLiteral("Active"));
    it = changedProps.constFind(QStringLiteral("IdleHint"));
    if (it != changedProps.cend()) {
        m_login1SessionIdleHint = it->toBool();
    }
}

void PowerDevilUPowerBackend::animationValueChanged(const QVariant &value)
{
    if (m_ddcBrightnessControl->isSupported()) {
        m_ddcBrightnessControl->setBrightness(m_ddcBrightnessControl->displayIds().constFirst(), value.toInt());
    } else {
        qCInfo(POWERDEVIL) << "PowerDevilUPowerBackend::animationValueChanged: brightness control not supported";
    }
}

int PowerDevilUPowerBackend::keyboardBrightness() const
{
    int result = m_kbdBacklight->GetBrightness();
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value: " << result;

    return result;
}

#include "moc_powerdevilupowerbackend.cpp"
