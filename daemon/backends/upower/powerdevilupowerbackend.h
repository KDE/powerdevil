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

#include <kdemacros.h>

#include "upower_device_interface.h"
#include "upower_interface.h"
#include "upower_kbdbacklight_interface.h"

#define UPOWER_SERVICE "org.freedesktop.UPower"

class XRandrBrightness;

class KDE_EXPORT PowerDevilUPowerBackend : public PowerDevil::BackendInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilUPowerBackend)
public:
    explicit PowerDevilUPowerBackend(QObject* parent);
    virtual ~PowerDevilUPowerBackend();

    virtual void init();
    static bool isAvailable();

    virtual float brightness(BrightnessControlType type = Screen) const;

    virtual void brightnessKeyPressed(PowerDevil::BackendInterface::BrightnessKeyType type);
    virtual bool setBrightness(float brightness, PowerDevil::BackendInterface::BrightnessControlType type = Screen);
    virtual KJob* suspend(PowerDevil::BackendInterface::SuspendMethod method);

private:
    void enumerateDevices();

private slots:
    void updateDeviceProps();
    void slotDeviceAdded(const QString &);
    void slotDeviceRemoved(const QString &);
    void slotDeviceChanged(const QString &);
    void slotPropertyChanged();

private:
    // upower devices
    QMap<QString, OrgFreedesktopUPowerDeviceInterface *> m_devices;

    // brightness
    float m_cachedBrightness;
    XRandrBrightness         *m_brightnessControl;
    OrgFreedesktopUPowerInterface *m_upowerInterface;
    OrgFreedesktopUPowerKbdBacklightInterface *m_kbdBacklight;

    // buttons
    bool m_lidIsPresent;
    bool m_lidIsClosed;
    bool m_onBattery;
};

#endif // POWERDEVILUPOWERBACKEND_H
