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

#ifndef POWERDEVILHALBACKEND_H
#define POWERDEVILHALBACKEND_H

#include <powerdevilbackendinterface.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>

#include <kdemacros.h>

#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/button.h>

class XRandrBrightness;
class OrgFreedesktopUPowerInterface;
namespace Solid {
class Device;
}


class KDE_EXPORT PowerDevilUPowerBackend : public PowerDevil::BackendInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilUPowerBackend)
public:
    explicit PowerDevilUPowerBackend(QObject* parent, const QVariantList&);
    virtual ~PowerDevilUPowerBackend();

    virtual void init();

    virtual float brightness(BrightnessControlType type = Screen) const;

    virtual void brightnessKeyPressed(PowerDevil::BackendInterface::BrightnessKeyType type);
    virtual bool setBrightness(float brightness, PowerDevil::BackendInterface::BrightnessControlType type = Screen);
    virtual KJob* suspend(PowerDevil::BackendInterface::SuspendMethod method);

private:
    void computeAcAdapters();
    void computeBatteries();
    void computeButtons();

private slots:
    void updateBatteryStats();
    void slotPlugStateChanged(bool newState);
    void slotButtonPressed(Solid::Button::ButtonType type);
    void slotDeviceAdded(const QString &udi);
    void slotDeviceRemoved(const QString &udi);
    void slotBatteryChargeChanged();

private:
    QMap<QString, Solid::Device *> m_acAdapters;
    QMap<QString, Solid::Device *> m_batteries;
    QMap<QString, Solid::Device *> m_buttons;

    int m_pluggedAdapterCount;

    int m_estimatedBatteryTime;

    bool m_brightnessInHardware;
    float m_cachedBrightness;

    //Custom UPower stuff
    XRandrBrightness              *m_brightNessControl;
    OrgFreedesktopUPowerInterface *m_upowerInterface;
};

#endif // POWERDEVILHALBACKEND_H
