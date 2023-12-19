/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <powerdevilbackendinterface.h>

#include <QDBusConnection>

#include <map>
#include <memory>

#include "udevqt.h"

#include "powerdevilupowerbackend_export.h"

#define UPOWER_SERVICE "org.freedesktop.UPower"

class QPropertyAnimation;
class QTimer;
class DDCutilBrightness;
class POWERDEVILUPOWERBACKEND_EXPORT PowerDevilUPowerBackend : public PowerDevil::BackendInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilUPowerBackend)
    Q_PLUGIN_METADATA(IID "org.kde.powerdevil.upowerbackend");

public:
    explicit PowerDevilUPowerBackend(QObject *parent = nullptr);
    ~PowerDevilUPowerBackend() override;

    void init() override;

    int screenBrightness() const override;
    int screenBrightnessMax() const override;
    void setScreenBrightness(int value) override;
    bool screenBrightnessAvailable() const override;

    int screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type) override;

Q_SIGNALS:
    void brightnessSupportQueried(bool available);

private:
    void enumerateDevices();
    void addDevice(const QString &);

private Q_SLOTS:
    void slotScreenBrightnessChanged();
    void onDeviceChanged(const UdevQt::Device &device);

private:
    void animationValueChanged(const QVariant &value);
    void initWithBrightness(bool brightnessSupport);

    // brightness
    int m_cachedScreenBrightness;
    bool m_screenBrightnessAvailable = false;

    DDCutilBrightness *m_ddcBrightnessControl = nullptr;

    int m_brightnessMax = 0;

    QPropertyAnimation *m_brightnessAnimation = nullptr;
    const int m_brightnessAnimationDurationMsec = 250;
    const int m_brightnessAnimationThreshold = 100;

    QTimer *m_brightnessAnimationTimer = nullptr;

    // property if brightness control is leds subsystem
    bool m_isLedBrightnessControl;

    // helper path
    QString m_syspath;
};
