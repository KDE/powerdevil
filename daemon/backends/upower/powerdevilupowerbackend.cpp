/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    Copyright (C) 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    Copyright (C) 2016-17 Bhushan Shah <bshah@kde.org>

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
#include <KPluginFactory>
#include <KSharedConfig>
#include <KAuthAction>

#include "xrandrxcbhelper.h"
#include "sysfsbrightness.h"
#include "xrandrbrightness.h"
#include "ddcutilbrightness.h"
#include "upowersuspendjob.h"
#include "login1suspendjob.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent)
    : BackendInterface(parent)
    , m_displayDevice(Q_NULLPTR)
    , m_brightnessControl(Q_NULLPTR)
    , m_randrHelper(Q_NULLPTR)
    , m_upowerInterface(Q_NULLPTR)
    , m_kbdBacklight(Q_NULLPTR)
    , m_kbdMaxBrightness(0)
    , m_lidIsPresent(false)
    , m_lidIsClosed(false)
    , m_onBattery(false)
{

}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend()
{
    delete m_brightnessControl;
    delete m_ddcBrightnessControl;
    delete m_sysfsBrightnessControl;
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

    m_brightnessControl = new XRandrBrightness();

    m_ddcBrightnessControl = new DDCutilBrightness();
    m_ddcBrightnessControl->detect();

    m_sysfsBrightnessControl = new SysfsBrightness();
    m_sysfsBrightnessControl->detect();

    bool brightnessSupport = false;
    if (m_brightnessControl->isSupported()) {
        qCDebug(POWERDEVIL) << "Using XRandr interface for adjusting brightness";
        m_randrHelper = XRandRXCBHelper::self();
        Q_ASSERT(m_randrHelper);
        connect(m_randrHelper, &XRandRXCBHelper::brightnessChanged, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
        brightnessSupport = true;
    } else if (m_ddcBrightnessControl->isSupported()) {
        qCDebug(POWERDEVIL) << "Using DDCutillib for adjusting brightness";
        m_cachedBrightnessMap.insert(Screen, brightness(Screen));
        brightnessSupport = true;
    } else if (m_sysfsBrightnessControl->isSupported()) {
        qCDebug(POWERDEVIL) << "Using sysfs interface for adjusting brightness";
        m_cachedBrightnessMap.insert(Screen, m_sysfsBrightnessControl->brightness());
        m_brightnessMax = m_sysfsBrightnessControl->brightnessMax();
        brightnessSupport = true;
    }

    if (brightnessSupport) {
        const int duration = PowerDevilSettings::brightnessAnimationDuration();
        if (duration > 0 && brightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
            m_brightnessAnimation = new QPropertyAnimation(this);
            m_brightnessAnimation->setTargetObject(this);
            m_brightnessAnimation->setDuration(duration);
            m_brightnessAnimation->setEasingCurve(QEasingCurve::InOutQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
        }
        Q_EMIT brightnessSupportQueried(true);
    } else {
        Q_EMIT brightnessSupportQueried(false);
    }
}

void PowerDevilUPowerBackend::initWithBrightness(bool screenBrightnessAvailable)
{
    disconnect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    // Capabilities
    setCapabilities(SignalResumeFromSuspend);

    // devices
    enumerateDevices();

    connect(m_upowerInterface, SIGNAL(Changed()), this, SLOT(slotPropertyChanged()));
    // for UPower >= 0.99.0, missing Changed() signal
    QDBusConnection::systemBus().connect(UPOWER_SERVICE, UPOWER_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged", this,
                                         SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

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
    }

    /* There's a chance we're using ConsoleKit rather than ConsoleKit2 as the
     * m_login1Interface, so check if we can suspend/hibernate with UPower < 0.99 */
    if (supported == UnknownSuspendMethod) {
        if (m_upowerInterface->canSuspend() && m_upowerInterface->SuspendAllowed()) {
            qCDebug(POWERDEVIL) << "Can suspend";
            supported |= ToRam;
        }

        if (m_upowerInterface->canHibernate() && m_upowerInterface->HibernateAllowed()) {
            qCDebug(POWERDEVIL) << "Can hibernate";
            supported |= ToDisk;
        }

        if (supported != UnknownSuspendMethod) {
            m_useUPowerSuspend = true;
        }
    }

    // "resuming" signal
    if (m_login1Interface && !m_useUPowerSuspend) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1PrepareForSleep(bool)));
    } else {
        connect(m_upowerInterface, &OrgFreedesktopUPowerInterface::Sleeping, this, &PowerDevilUPowerBackend::aboutToSuspend);
        connect(m_upowerInterface, &OrgFreedesktopUPowerInterface::Resuming, this, &PowerDevilUPowerBackend::resumeFromSuspend);
    }

    // battery
    Q_FOREACH(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
        if (upowerDevice->type() == 2 && upowerDevice->powerSupply()) {
            QString udi = upowerDevice->path();
            setCapacityForBattery(udi, qRound(upowerDevice->capacity()));  // acknowledge capacity
        }
    }

    // backend ready
    setBackendIsReady(controls, supported);
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
        if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
            result = m_brightnessAnimation->endValue().toInt();
        } else {
            //qCDebug(POWERDEVIL) << "Calling xrandr brightness";
            if (m_brightnessControl->isSupported()) {
                result = (int) m_brightnessControl->brightness();
            } else if (m_ddcBrightnessControl->isSupported()){
                result = (int)m_ddcBrightnessControl->brightness();
            } else {
                result = m_sysfsBrightnessControl->brightness();
            }
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
        } else if (m_ddcBrightnessControl->isSupported()){
            result = (int)m_ddcBrightnessControl->brightnessMax();
        }else{
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
        if (m_brightnessAnimation) {
            m_brightnessAnimation->stop();
            disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->setStartValue(brightness());
            m_brightnessAnimation->setEndValue(value);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->start();
        } else {
            if (m_brightnessControl->isSupported()) {
                m_brightnessControl->setBrightness(value);
            } else if (m_ddcBrightnessControl->isSupported()) {
                m_ddcBrightnessControl->setBrightness((long)value);
            } else if (m_sysfsBrightnessControl->isSupported()) {
                m_sysfsBrightnessControl->setBrightness((long)value);
            }
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

KJob* PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    if (m_login1Interface && !m_useUPowerSuspend) {
        return new Login1SuspendJob(m_login1Interface.data(), method, supportedSuspendMethods());
    } else {
        return new UPowerSuspendJob(m_upowerInterface, method, supportedSuspendMethods());
    }
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    setLidPresent(m_lidIsPresent);
    m_lidIsClosed = m_upowerInterface->lidIsClosed();
    m_onBattery = m_upowerInterface->onBattery();

    QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
    Q_FOREACH (const QDBusObjectPath & device, deviceList) {
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
    qlonglong remainingTime = 0;

    if (m_displayDevice && m_displayDevice->isPresent()) {
        const uint state = m_displayDevice->state();
        if (state == 1) // charging
            remainingTime = m_displayDevice->timeToFull();
        else if (state == 2) //discharging
            remainingTime = m_displayDevice->timeToEmpty();
    } else {
        qreal energyTotal = 0.0;
        qreal energyRateTotal = 0.0;
        qreal energyFullTotal = 0.0;
        uint stateTotal = 0;

        Q_FOREACH(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
            const uint type = upowerDevice->type();
            if (( type == 2 || type == 3) && upowerDevice->powerSupply()) {
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

void PowerDevilUPowerBackend::slotPropertyChanged()
{
    // check for lid button changes
    if (m_lidIsPresent) {
        const bool lidIsClosed = m_upowerInterface->lidIsClosed();
        if (lidIsClosed != m_lidIsClosed) {
            if (lidIsClosed)
                setButtonPressed(LidClose);
            else
                setButtonPressed(LidOpen);
        }
        m_lidIsClosed = lidIsClosed;
    }

    // check for AC adapter changes
    const bool onBattery = m_upowerInterface->onBattery();
    if (m_onBattery != onBattery) {
        if (onBattery)
            setAcAdapterState(Unplugged);
        else
            setAcAdapterState(Plugged);
    }

    m_onBattery = onBattery;
}

void PowerDevilUPowerBackend::onPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    Q_UNUSED(changedProps);
    Q_UNUSED(invalidatedProps);

    if (ifaceName == UPOWER_IFACE) {
        slotPropertyChanged(); // TODO maybe process the 2 properties separately?
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
    if (m_brightnessControl->isSupported()) {
        m_brightnessControl->setBrightness(value.toInt());
    } else if (m_ddcBrightnessControl->isSupported()) {
        m_ddcBrightnessControl->setBrightness(value.toInt());
    } else if (m_sysfsBrightnessControl->isSupported()) {
        m_sysfsBrightnessControl->setBrightness(value.toInt());
    } else {
        qCInfo(POWERDEVIL)<<"PowerDevilUPowerBackend::animationValueChanged: brightness control not supported";
    }
}
