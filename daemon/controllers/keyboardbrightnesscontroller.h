/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

 */

#pragma once

#include <QObject>

#include "powerdevilcore_export.h"
#include "powerdevilenums.h"
#include "powerdevilkeyboardbrightnesslogic.h"
#include "upower_kbdbacklight_interface.h"

class POWERDEVILCORE_EXPORT KeyboardBrightnessController : public QObject
{
    Q_OBJECT
public:
    KeyboardBrightnessController();

    int keyboardBrightness() const;
    int keyboardBrightnessMax() const;
    void setKeyboardBrightness(int value);
    void setKeyboardBrightnessOff();
    bool keyboardBrightnessAvailable() const;
    int keyboardBrightnessSteps();

    int keyboardBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type);

Q_SIGNALS:
    void keyboardBrightnessChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &brightnessInfo);

private Q_SLOTS:
    void onKeyboardBrightnessChanged(int value, const QString &source);

private:
    int calculateNextKeyboardBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType);

    int m_kbdMaxBrightness;
    int m_cachedKeyboardBrightness;
    int m_keyboardBrightnessBeforeTogglingOff;
    bool m_keyboardBrightnessAvailable = false;

    PowerDevil::KeyboardBrightnessLogic m_keyboardBrightnessLogic;

    OrgFreedesktopUPowerKbdBacklightInterface *m_kbdBacklight;
};
