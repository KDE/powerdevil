/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <QDBusConnection>

#include <map>
#include <memory>

#include "powerdevilscreenbrightnesslogic.h"
#include "udevqt.h"

#include "powerdevilcore_export.h"

class QPropertyAnimation;
class QTimer;
class DDCutilBrightness;
class POWERDEVILCORE_EXPORT ScreenBrightnessController : public QObject
{
    Q_OBJECT
public:
    ScreenBrightnessController();

    int screenBrightness() const;
    int screenBrightnessMax() const;
    void setScreenBrightness(int value);
    bool screenBrightnessAvailable() const;
    int screenBrightnessSteps();

    int screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type);

Q_SIGNALS:
    void brightnessSupportQueried(bool available);
    void detectionFinished();
    void screenBrightnessChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &brightnessInfo);

private:
    void enumerateDevices();
    void addDevice(const QString &);
    int calculateNextScreenBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType);

private Q_SLOTS:
    void initWithBrightness(bool screenBrightnessAvailable);
    void slotScreenBrightnessChanged();
    void onDeviceChanged(const UdevQt::Device &device);
    void onScreenBrightnessChanged(int value, int valueMax);

private:
    void animationValueChanged(const QVariant &value);

    // brightness
    int m_cachedScreenBrightness;
    bool m_screenBrightnessAvailable = false;
    PowerDevil::ScreenBrightnessLogic m_screenBrightnessLogic;

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
