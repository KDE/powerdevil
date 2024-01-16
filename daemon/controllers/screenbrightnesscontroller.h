/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <QObject>

#include <utility> // std::pair

#include <powerdevilcore_export.h>

#include "displaybrightness.h"
#include "powerdevilscreenbrightnesslogic.h"

class BacklightDetector;
class DDCutilDetector;

class POWERDEVILCORE_EXPORT ScreenBrightnessController : public QObject
{
    Q_OBJECT
public:
    ScreenBrightnessController();

    void detectDisplays(); // will emit detectionFinished signal once all candidates were checked

    int screenBrightness() const;
    int screenBrightnessMax() const;
    void setScreenBrightness(int value);
    bool screenBrightnessAvailable() const;
    int screenBrightnessSteps();

    int screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type);

Q_SIGNALS:
    void detectionFinished();
    void screenBrightnessChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &brightnessInfo);

private:
    int calculateNextScreenBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType);

private Q_SLOTS:
    void onDisplaysChanged();
    void onScreenBrightnessChanged(int value, int valueMax);

private:
    QList<DisplayBrightness *> m_displays;
    PowerDevil::ScreenBrightnessLogic m_screenBrightnessLogic;

    QList<std::pair<DisplayBrightnessDetector *, const char * /*debug name*/>> m_detectors;
    int m_finishedDetectingCount = 0;
};
