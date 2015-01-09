/*  This file is part of the KDE project
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008-2010 Dario Freddi <drf@kde.org>
    Copyright (C) 2010 Alejandro Fiestas <alex@eyeos.org>

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

#ifndef POWERDEVILUPOWERBACKEND_H
#define POWERDEVILUPOWERBACKEND_H

#include <powerdevilbackendinterface.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>

#include "upower_device_interface.h"
#include "upower_interface.h"
#include "upower_kbdbacklight_interface.h"
#include "udevqt.h"

#define UPOWER_SERVICE "org.freedesktop.UPower"
#define UPOWER_PATH "/org/freedesktop/UPower"
#define UPOWER_IFACE "org.freedesktop.UPower"
#define UPOWER_IFACE_DEVICE "org.freedesktop.UPower.Device"

#define LOGIN1_SERVICE "org.freedesktop.login1"

class UdevHelper;
class XRandRXCBHelper;
class XRandrBrightness;

class Q_DECL_EXPORT PowerDevilUPowerBackend : public PowerDevil::BackendInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilUPowerBackend)
public:
    explicit PowerDevilUPowerBackend(QObject* parent);
    virtual ~PowerDevilUPowerBackend();

    virtual void init();
    static bool isAvailable();

    virtual int brightnessValue(BrightnessControlType type = Screen) const;
    virtual int brightnessValueMax(BrightnessControlType type = Screen) const;

    virtual void brightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type, BrightnessControlType controlType);
    virtual bool setBrightnessValue(int brightnessValue, PowerDevil::BackendInterface::BrightnessControlType type = Screen);
    virtual KJob* suspend(PowerDevil::BackendInterface::SuspendMethod method);

private:
    void enumerateDevices();
    void addDevice(const QString &);

private slots:
    void updateDeviceProps();
    void slotDeviceAdded(const QString &);
    void slotDeviceRemoved(const QString &);
    void slotDeviceAdded(const QDBusObjectPath & path);
    void slotDeviceRemoved(const QDBusObjectPath & path);
    void slotDeviceChanged(const QString &);
    void slotPropertyChanged();
    void slotLogin1Resuming(bool active);
    void slotScreenBrightnessChanged();
    void onDeviceChanged(const UdevQt::Device &device);
    void onKeyboardBrightnessChanged(int);

    void onPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);
    void onDevicePropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);

private:
    // upower devices
    QMap<QString, OrgFreedesktopUPowerDeviceInterface *> m_devices;
    OrgFreedesktopUPowerDeviceInterface *m_displayDevice;

    // brightness
    QMap<BrightnessControlType, int> m_cachedBrightnessMap;
    XRandrBrightness         *m_brightnessControl;
    XRandRXCBHelper *m_randrHelper;

    OrgFreedesktopUPowerInterface *m_upowerInterface;
    OrgFreedesktopUPowerKbdBacklightInterface *m_kbdBacklight;
    int m_kbdMaxBrightness;

    // login1 interface
    QWeakPointer<QDBusInterface> m_login1Interface;

    // buttons
    bool m_lidIsPresent;
    bool m_lidIsClosed;
    bool m_onBattery;

    //helper path
    QString m_syspath;
};

#endif // POWERDEVILUPOWERBACKEND_H
