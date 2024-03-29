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
#include "ddcutildetector.h"

ScreenBrightnessController::ScreenBrightnessController()
    : QObject()
    , m_detectors({
          {new BacklightDetector(this), "internal display backlight"},
          {new DDCutilDetector(this), "libddcutil"},
      })
{
}

void ScreenBrightnessController::detectDisplays()
{
    disconnect(nullptr, &DisplayBrightness::brightnessChanged, this, &ScreenBrightnessController::onScreenBrightnessChanged);
    disconnect(nullptr, &DisplayBrightnessDetector::displaysChanged, this, &ScreenBrightnessController::onDisplaysChanged);

    qCDebug(POWERDEVIL) << "Trying to detect displays for brightness control...";
    m_finishedDetectingCount = 0;
    m_displays.clear();

    for (const std::pair<DisplayBrightnessDetector *, const char *> &detectorNamePair : m_detectors) {
        DisplayBrightnessDetector *detector = detectorNamePair.first;

        connect(detector, &DisplayBrightnessDetector::detectionFinished, this, [this, detector]() {
            disconnect(detector, &DisplayBrightnessDetector::detectionFinished, this, nullptr);

            if (++m_finishedDetectingCount; m_finishedDetectingCount == m_detectors.size()) {
                onDisplaysChanged();
                Q_EMIT detectionFinished();
            }
            connect(detector, &DisplayBrightnessDetector::displaysChanged, this, &ScreenBrightnessController::onDisplaysChanged);
        });
        detector->detect();
    }
}

void ScreenBrightnessController::onDisplaysChanged()
{
    DisplayBrightness *previousFirstDisplay = m_displays.isEmpty() ? nullptr : m_displays.first();
    m_displays.clear();

    for (const auto &detectorNamePair : m_detectors) {
        if (m_displays += detectorNamePair.first->displays(); !m_displays.isEmpty()) {
            qCDebug(POWERDEVIL) << "Using" << detectorNamePair.second << "for brightness controls.";
            break; // FIXME: remove this and use all available displays once we have a UI/API for it
        }
    }
    if (m_displays.isEmpty()) {
        qCDebug(POWERDEVIL) << "No suitable displays detected. Brightness controls are unsupported in this configuration.";
    }

    DisplayBrightness *newFirstDisplay = m_displays.isEmpty() ? nullptr : m_displays.first();

    if (previousFirstDisplay != nullptr && newFirstDisplay != previousFirstDisplay) {
        disconnect(previousFirstDisplay, &DisplayBrightness::brightnessChanged, this, &ScreenBrightnessController::onScreenBrightnessChanged);
    }
    if (newFirstDisplay != nullptr) {
        // ScreenBrightnessController's API can only deal with a single display right now.
        // We'll use the first one.
        connect(newFirstDisplay, &DisplayBrightness::brightnessChanged, this, &ScreenBrightnessController::onScreenBrightnessChanged);

        if (newFirstDisplay != previousFirstDisplay) {
            onScreenBrightnessChanged(screenBrightness(), screenBrightnessMax());
            qCDebug(POWERDEVIL) << "screen brightness value after display detection/reconfiguration:" << screenBrightness();
        }
    }
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
    if (!screenBrightnessAvailable()) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = screenBrightness();
    // m_cachedBrightness is not being updated during animation, thus checking the m_cachedBrightness
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.

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
    if (!screenBrightnessAvailable()) {
        return 0;
    }
    int result = m_displays.first()->brightness();
    qCDebug(POWERDEVIL) << "Screen brightness value:" << result;
    return result;
}

int ScreenBrightnessController::screenBrightnessMax() const
{
    if (!screenBrightnessAvailable()) {
        return 0;
    }
    int result = m_displays.first()->maxBrightness();
    qCDebug(POWERDEVIL) << "Screen brightness value max:" << result;
    return result;
}

void ScreenBrightnessController::setScreenBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set screen brightness value:" << value;
    for (DisplayBrightness *display : std::as_const(m_displays)) {
        display->setBrightness(value);
    }
}

bool ScreenBrightnessController::screenBrightnessAvailable() const
{
    return !m_displays.isEmpty();
}

void ScreenBrightnessController::onScreenBrightnessChanged(int value, int valueMax)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

#include "moc_screenbrightnesscontroller.cpp"
