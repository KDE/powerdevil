/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>
    Copyright (C) 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>

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
#include <KAuth/Action>

#include "xrandrx11helper.h"
#include "xrandrbrightness.h"
#include "upowersuspendjob.h"
#include "login1suspendjob.h"
#include "udevqt.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

bool checkSystemdVersion(uint requiredVersion)
{
    bool ok;

    QDBusInterface systemdIface("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                                QDBusConnection::systemBus(), 0);
    uint version = systemdIface.property("Version").toString().section(' ', 1).toUInt(&ok);
    if (ok) {
       return (version >= requiredVersion);
    } else {
       kDebug() << "Unknown version string from Systemd";
       return false;
    }
}

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject* parent)
    : BackendInterface(parent),
      m_brightnessControl(0),
      m_lidIsPresent(false), m_lidIsClosed(false), m_onBattery(false)
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
        kDebug() << "UPower service, " << UPOWER_SERVICE << ", is not registered on the bus. Trying to find out if it is activated.";
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DBus",
                                                              "/org/freedesktop/DBus",
                                                              "org.freedesktop.DBus",
                                                              "ListActivatableNames");

        QDBusPendingReply< QStringList > reply = QDBusConnection::systemBus().asyncCall(message);
        reply.waitForFinished();

        if (reply.isValid()) {
            if (reply.value().contains(UPOWER_SERVICE)) {
                kDebug() << "UPower was found, activating service...";
                QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
                return true;
            } else {
                kDebug() << "UPower cannot be found on this system.";
                return false;
            }
        } else {
            kWarning() << "Could not request activatable names to DBus!";
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

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        m_login1Interface = new QDBusInterface(LOGIN1_SERVICE, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus(), this);
    }
    m_upowerInterface = new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);
    m_kbdBacklight = new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), this);
    m_brightnessControl = new XRandrBrightness();
    if (!m_brightnessControl->isSupported()) {
        qDebug() << "Using helper";
        KAuth::Action action("org.kde.powerdevil.backlighthelper.syspath");
        action.setHelperID(HELPER_ID);
        KAuth::ActionReply reply = action.execute();
        if (reply.succeeded()) {
            m_syspath = reply.data()["syspath"].toString();
            m_syspath = QFileInfo(m_syspath).readLink();
        }
    } else {
        qDebug() << "Using XRandR";
    }
    m_randrHelper = new XRandRX11Helper();
    UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
    connect(client, SIGNAL(deviceChanged(UdevQt::Device)), SLOT(deviceChanged(UdevQt::Device)));

    connect(m_randrHelper, SIGNAL(brightnessChanged()), this, SLOT(slotScreenBrightnessChanged()));

    // Capabilities
    setCapabilities(SignalResumeFromSuspend);

    // devices
    enumerateDevices();
    connect(m_upowerInterface, SIGNAL(Changed()), this, SLOT(slotPropertyChanged()));
    connect(m_upowerInterface, SIGNAL(DeviceAdded(QString)), this, SLOT(slotDeviceAdded(QString)));
    connect(m_upowerInterface, SIGNAL(DeviceRemoved(QString)), this, SLOT(slotDeviceRemoved(QString)));
    connect(m_upowerInterface, SIGNAL(DeviceChanged(QString)), this, SLOT(slotDeviceChanged(QString)));

    // Brightness Controls available
    BrightnessControlsList controls;
    controls.insert(QLatin1String("LVDS1"), Screen);
    m_cachedBrightnessMap.insert(Screen, brightness(Screen));
    kDebug() << "current screen brightness: " << m_cachedBrightnessMap.value(Screen);

    if (m_kbdBacklight->isValid()) {
        controls.insert(QLatin1String("KBD"), Keyboard);
        m_cachedBrightnessMap.insert(Keyboard, brightness(Keyboard));
        kDebug() << "current keyboard backlight brightness: " << m_cachedBrightnessMap.value(Keyboard);
    }

    // Supported suspend methods
    SuspendMethods supported = UnknownSuspendMethod;
    if (m_login1Interface && checkSystemdVersion(195)) {
        QDBusPendingReply<QString> canSuspend = m_login1Interface.data()->asyncCall("CanSuspend");
        canSuspend.waitForFinished();
        if (canSuspend.isValid() && (canSuspend.value() == "yes" || canSuspend.value() == "challenge"))
            supported |= ToRam;

        QDBusPendingReply<QString> canHibernate = m_login1Interface.data()->asyncCall("CanHibernate");
        canHibernate.waitForFinished();
        if (canHibernate.isValid() && (canHibernate.value() == "yes" || canHibernate.value() == "challenge"))
            supported |= ToDisk;

        QDBusPendingReply<QString> canHybridSleep = m_login1Interface.data()->asyncCall("CanHybridSleep");
        canHybridSleep.waitForFinished();
        if (canHybridSleep.isValid() && (canHybridSleep.value() == "yes" || canHybridSleep.value() == "challenge"))
            supported |= HybridSuspend;
    } else {
        if (m_upowerInterface->canSuspend() && m_upowerInterface->SuspendAllowed()) {
            kDebug() << "Can suspend";
            supported |= ToRam;
        }

        if (m_upowerInterface->canHibernate() && m_upowerInterface->HibernateAllowed()) {
            kDebug() << "Can hibernate";
            supported |= ToDisk;
        }
    }

    // "resuming" signal
    if (m_login1Interface && checkSystemdVersion(198)) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1Resuming(bool)));
    } else {
        connect(m_upowerInterface, SIGNAL(Resuming()), this, SIGNAL(resumeFromSuspend()));
    }

    // battery
    QList<RecallNotice> recallList;
    foreach(OrgFreedesktopUPowerDeviceInterface * upowerDevice, m_devices) {
        if (upowerDevice->type() == 2 && upowerDevice->powerSupply()) {
            QString udi = upowerDevice->path();
            setCapacityForBattery(udi, qRound(upowerDevice->capacity()));  // acknowledge capacity

            if (upowerDevice->recallNotice()) {                            // check for recall notices
                RecallNotice notice;
                notice.batteryId = udi;
                notice.url = upowerDevice->recallUrl();
                notice.vendor = upowerDevice->recallVendor();

                recallList.append(notice);
            }
        }
    }
    if (!recallList.isEmpty())
        setRecallNotices(recallList);

    // backend ready
    setBackendIsReady(controls, supported);
}

void PowerDevilUPowerBackend::deviceChanged(const UdevQt::Device &device)
{
    qDebug() << "Udev device changed";
    qDebug() << m_syspath;
    qDebug() << device.sysfsPath();
    if (device.sysfsPath() != m_syspath) {
        return;
    }

    int maxBrightness = device.sysfsProperty("max_brightness").toInt();
    float newBrightness = device.sysfsProperty("brightness").toInt() * 100 / maxBrightness;

    if (!qFuzzyCompare(newBrightness, m_cachedBrightnessMap[Screen])) {
        m_cachedBrightnessMap[Screen] = newBrightness;
        onBrightnessChanged(Screen, m_cachedBrightnessMap[Screen]);
    }
}

void PowerDevilUPowerBackend::brightnessKeyPressed(PowerDevil::BackendInterface::BrightnessKeyType type, BrightnessControlType controlType)
{
    BrightnessControlsList allControls = brightnessControlsAvailable();
    QList<QString> controls = allControls.keys(controlType);

    if (controls.isEmpty()) {
        return; // ignore as we are not able to determine the brightness level
    }

    if (type == Toggle && controlType == Screen) {
        return; // ignore as we wont toggle the screen off
    }

    float currentBrightness = brightness(controlType);

    int step = 10;
    if (controlType == Keyboard) {
        // In case the keyboard backlight has only 5 or less possible values,
        // 10% are not enough to hit the next value. Lets use 30% because
        // that jumps exactly one value for 2, 3, 4 and 5 possible steps
        // when rounded.
        if (m_kbdBacklight->GetMaxBrightness() < 6) {
            step = 30;
        }
    }

    if (qFuzzyCompare(currentBrightness, m_cachedBrightnessMap.value(controlType))) {
        float newBrightness;
        if (type == Increase) {
            newBrightness = qMin(100.0f, currentBrightness + step);
        } else if (type == Decrease) {
            newBrightness = qMax(0.0f, currentBrightness - step);
        } else { // Toggle On/off
            newBrightness = currentBrightness > 0 ? 0 : 100;
        }

        setBrightness(newBrightness, controlType);
    } else {
        m_cachedBrightnessMap[controlType] = currentBrightness;
    }
}

float PowerDevilUPowerBackend::brightness(PowerDevil::BackendInterface::BrightnessControlType type) const
{
    float result = 0.0;

    if (type == Screen) {
        if (m_brightnessControl->isSupported()) {
            //kDebug() << "Calling xrandr brightness";
            result = m_brightnessControl->brightness();
        } else {
            //kDebug() << "Falling back to helper to get brightness";
            KAuth::Action action("org.kde.powerdevil.backlighthelper.brightness");
            action.setHelperID(HELPER_ID);
            KAuth::ActionReply reply = action.execute();
            if (reply.succeeded()) {
                result = reply.data()["brightness"].toFloat();
                //kDebug() << "org.kde.powerdevil.backlighthelper.brightness succeeded: " << reply.data()["brightness"];
            }
            else
                kWarning() << "org.kde.powerdevil.backlighthelper.brightness failed";

        }
        kDebug() << "Screen brightness: " << result;
    } else if (type == Keyboard) {
        kDebug() << "Kbd backlight brightness: " << m_kbdBacklight->GetBrightness();
        result = 1.0 * m_kbdBacklight->GetBrightness() / m_kbdBacklight->GetMaxBrightness() * 100;
    }

    return result;
}

bool PowerDevilUPowerBackend::setBrightness(float brightnessValue, PowerDevil::BackendInterface::BrightnessControlType type)
{
    bool success = false;
    if (type == Screen) {
        kDebug() << "set screen brightness: " << brightnessValue;
        if (m_brightnessControl->isSupported()) {
            m_brightnessControl->setBrightness(brightnessValue);
        } else {
            //kDebug() << "Falling back to helper to set brightness";
            KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
            action.setHelperID(HELPER_ID);
            action.addArgument("brightness", brightnessValue);
            KAuth::ActionReply reply = action.execute();
            if (reply.failed()) {
                kWarning() << "org.kde.powerdevil.backlighthelper.setbrightness failed";
                return false;
            }
        }

        success = true;
    } else if (type == Keyboard) {
        kDebug() << "set kbd backlight: " << brightnessValue;
        m_kbdBacklight->SetBrightness(brightnessValue / 100 * m_kbdBacklight->GetMaxBrightness());
        success = true;
    }

    return success;
}

void PowerDevilUPowerBackend::slotScreenBrightnessChanged()
{
    float newBrightness = brightness(Screen);
    if (!qFuzzyCompare(newBrightness, m_cachedBrightnessMap[Screen])) {
        m_cachedBrightnessMap[Screen] = newBrightness;
        onBrightnessChanged(Screen, m_cachedBrightnessMap[Screen]);
    }
}

KJob* PowerDevilUPowerBackend::suspend(PowerDevil::BackendInterface::SuspendMethod method)
{
    if (m_login1Interface && checkSystemdVersion(195)) {
        return new Login1SuspendJob(m_login1Interface.data(), method, supportedSuspendMethods());
    } else {
        return new UPowerSuspendJob(m_upowerInterface, method, supportedSuspendMethods());
    }
}

void PowerDevilUPowerBackend::enumerateDevices()
{
    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    m_lidIsClosed = m_upowerInterface->lidIsClosed();
    m_onBattery = m_upowerInterface->onBattery();

    QList<QDBusObjectPath> deviceList = m_upowerInterface->EnumerateDevices();
    foreach (const QDBusObjectPath & device, deviceList) {
        OrgFreedesktopUPowerDeviceInterface * upowerDevice =
                new OrgFreedesktopUPowerDeviceInterface(UPOWER_SERVICE, device.path(), QDBusConnection::systemBus(), this);
        m_devices.insert(device.path(), upowerDevice);
    }

    updateDeviceProps();

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
        uint type = upowerDevice->type();
        if (( type == 2 || type == 3) && upowerDevice->powerSupply()) {
            uint state = upowerDevice->state();
            if (state == 1) // charging
                remainingTime += upowerDevice->timeToFull();
            else if (state == 2) //discharging
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
                setButtonPressed(LidClose);
            else
                setButtonPressed(LidOpen);
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

void PowerDevilUPowerBackend::slotLogin1Resuming(bool active)
{
    if (!active) {
        emit resumeFromSuspend();
    }
}

#include "powerdevilupowerbackend.moc"
