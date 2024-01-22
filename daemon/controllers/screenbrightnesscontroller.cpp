/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "screenbrightnesscontroller.h"

#include <brightnessosdwidget.h>
#include <powerdevil_debug.h>

#include <QDebug>
#include <QPropertyAnimation>

#include "backlightbrightness.h"
#include "ddcutilbrightness.h"

ScreenBrightnessController::ScreenBrightnessController()
    : QObject()
    , m_backlightBrightnessControl(new BacklightBrightness(this))
    , m_ddcBrightnessControl(new DDCutilBrightness(this))
{
    connect(m_backlightBrightnessControl, &BacklightBrightness::detectionFinished, this, &ScreenBrightnessController::onBacklightDetectionFinished);
    connect(m_backlightBrightnessControl, &BacklightBrightness::brightnessChanged, this, &ScreenBrightnessController::onScreenBrightnessChanged);

    qCDebug(POWERDEVIL) << "Trying to detect internal display backlight for brightness control...";
    m_backlightBrightnessControl->detect();
}

void ScreenBrightnessController::onBacklightDetectionFinished(bool isSupported)
{
    disconnect(m_backlightBrightnessControl, &BacklightBrightness::detectionFinished, this, &ScreenBrightnessController::onBacklightDetectionFinished);

    if (true) {
        qCDebug(POWERDEVIL) << "No internal dislay backlight detected. Trying DDC for brightness controls...";
        m_ddcBrightnessControl->detect();
        if (m_ddcBrightnessControl->isSupported()) {
            qCDebug(POWERDEVIL) << "Using DDCutillib";
            m_cachedBrightness = screenBrightness();
            if (m_brightnessAnimationDurationMsec > 0 && screenBrightnessMax() >= m_brightnessAnimationThreshold) {
                m_brightnessAnimation = new QPropertyAnimation(this);
                m_brightnessAnimation->setTargetObject(this);
                m_brightnessAnimation->setDuration(m_brightnessAnimationDurationMsec);
                connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
                connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &ScreenBrightnessController::ddcScreenBrightnessChanged);
            }
            m_screenBrightnessAvailable = true;
        } else if (isSupported) {
            m_screenBrightnessAvailable = true;
            m_cachedBrightness = m_backlightBrightnessControl->brightness();
        }
    }
    // Brightness Controls available
    if (m_screenBrightnessAvailable) {
        qCDebug(POWERDEVIL) << "initial screen brightness value:" << m_cachedBrightness;
    }
    Q_EMIT detectionFinished();
}

int ScreenBrightnessController::screenBrightnessSteps()
{
    m_screenBrightnessLogic.setValueMax(screenBrightnessMax());
    return m_screenBrightnessLogic.steps();
}

int ScreenBrightnessController::calculateNextScreenBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    return m_screenBrightnessLogic.action(keyType);
}

int ScreenBrightnessController::screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_screenBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = screenBrightness();
    // m_cachedBrightness is not being updated during animation, thus checking the m_cachedBrightness
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (!(m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) && currentBrightness != m_cachedBrightness) {
        m_cachedBrightness = currentBrightness;
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

int ScreenBrightnessController::screenBrightness() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
            result = m_brightnessAnimation->endValue().toInt();
        } else {
            result = m_ddcBrightnessControl->brightness(m_ddcBrightnessControl->displayIds().constFirst());
        }
    } else {
        result = m_cachedBrightness;
    }
    qCDebug(POWERDEVIL) << "Screen brightness value:" << result;
    return result;
}

int ScreenBrightnessController::screenBrightnessMax() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        result = m_ddcBrightnessControl->maxBrightness(m_ddcBrightnessControl->displayIds().constFirst());
    } else if (m_backlightBrightnessControl->isSupported()) {
        result = m_backlightBrightnessControl->maxBrightness();
    }
    qCDebug(POWERDEVIL) << "Screen brightness value max:" << result;

    return result;
}

void ScreenBrightnessController::setScreenBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set screen brightness value:" << value;
    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation) {
            m_brightnessAnimation->stop();
            disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
            m_brightnessAnimation->setStartValue(screenBrightness());
            m_brightnessAnimation->setEndValue(value);
            m_brightnessAnimation->setEasingCurve(screenBrightness() < value ? QEasingCurve::OutQuad : QEasingCurve::InQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
            m_brightnessAnimation->start();
        } else {
            for (const QString &displayId : m_ddcBrightnessControl->displayIds()) {
                m_ddcBrightnessControl->setBrightness(displayId, value);
            }
        }
    } else if (m_backlightBrightnessControl->isSupported()) {
        m_backlightBrightnessControl->setBrightness(value, m_brightnessAnimationDurationMsec);
    }
}

bool ScreenBrightnessController::screenBrightnessAvailable() const
{
    return m_screenBrightnessAvailable;
}

void ScreenBrightnessController::ddcScreenBrightnessChanged()
{
    if (m_brightnessAnimation && m_brightnessAnimation->state() != QPropertyAnimation::Stopped) {
        return;
    }

    if (int currentBrightness = screenBrightness(); currentBrightness != m_cachedBrightness) {
        onScreenBrightnessChanged(currentBrightness, screenBrightnessMax());
    }
}

void ScreenBrightnessController::animationValueChanged(const QVariant &value)
{
    if (m_ddcBrightnessControl->isSupported()) {
        for (const QString &displayId : m_ddcBrightnessControl->displayIds()) {
            m_ddcBrightnessControl->setBrightness(displayId, value.toInt());
        }
    } else {
        qCInfo(POWERDEVIL) << "ScreenBrightnessController::animationValueChanged: brightness control not supported";
    }
}

void ScreenBrightnessController::onScreenBrightnessChanged(int value, int valueMax)
{
    m_cachedBrightness = value;

    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

#include "moc_screenbrightnesscontroller.cpp"
