/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "keyboardbrightnesscontroller.h"

#include <powerdevil_debug.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

#include "brightnessosdwidget.h"

#define UPOWER_SERVICE "org.freedesktop.UPower"

KeyboardBrightnessController::KeyboardBrightnessController()
    : m_kbdMaxBrightness(0)
    , m_cachedKeyboardBrightness(0)
    , m_kbdBacklight(nullptr)
{
    m_kbdBacklight = new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), this);
    if (m_kbdBacklight->isValid()) {
        // Cache max value
        QDBusPendingReply<int> rep = m_kbdBacklight->GetMaxBrightness();
        rep.waitForFinished();
        if (rep.isValid()) {
            m_kbdMaxBrightness = rep.value();
            m_keyboardBrightnessAvailable = true;
        }
        // TODO Do a proper check if the kbd backlight dbus object exists. But that should work for now ..
        if (m_kbdMaxBrightness) {
            m_cachedKeyboardBrightness = keyboardBrightness();
            qCDebug(POWERDEVIL) << "current keyboard backlight brightness value: " << m_cachedKeyboardBrightness;
            connect(m_kbdBacklight,
                    &OrgFreedesktopUPowerKbdBacklightInterface::BrightnessChangedWithSource,
                    this,
                    &KeyboardBrightnessController::onKeyboardBrightnessChanged);
        }
    }
}

int KeyboardBrightnessController::keyboardBrightnessSteps()
{
    m_keyboardBrightnessLogic.setValueMax(keyboardBrightnessMax());
    return m_keyboardBrightnessLogic.steps();
}

int KeyboardBrightnessController::calculateNextKeyboardBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType)
{
    m_keyboardBrightnessLogic.setValueMax(valueMax);
    m_keyboardBrightnessLogic.setValue(value);
    m_keyboardBrightnessLogic.setValueBeforeTogglingOff(m_keyboardBrightnessBeforeTogglingOff);

    return m_keyboardBrightnessLogic.action(keyType);
}

int KeyboardBrightnessController::keyboardBrightness() const
{
    int result = m_kbdBacklight->GetBrightness();
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value: " << result;

    return result;
}

int KeyboardBrightnessController::keyboardBrightnessMax() const
{
    int result = m_kbdMaxBrightness;
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value max: " << result;

    return result;
}

void KeyboardBrightnessController::setKeyboardBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << value;
    m_kbdBacklight->SetBrightness(value);
    m_keyboardBrightnessBeforeTogglingOff = keyboardBrightness();
}

void KeyboardBrightnessController::setKeyboardBrightnessOff()
{
    // save value before toggling so that we can restore it later
    m_keyboardBrightnessBeforeTogglingOff = keyboardBrightness();
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << 0;
    m_kbdBacklight->SetBrightness(0);
}

bool KeyboardBrightnessController::keyboardBrightnessAvailable() const
{
    return m_keyboardBrightnessAvailable;
}

int KeyboardBrightnessController::keyboardBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_keyboardBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = keyboardBrightness();
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (currentBrightness != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = keyboardBrightnessMax();
    int newBrightness = calculateNextKeyboardBrightnessStep(currentBrightness, maxBrightness, type);

    if (newBrightness < 0) {
        return -1;
    }

    if (type == PowerDevil::BrightnessLogic::BrightnessKeyType::Toggle && newBrightness == 0) {
        setKeyboardBrightnessOff();
    } else {
        setKeyboardBrightness(newBrightness);
    }
    return newBrightness;
}

void KeyboardBrightnessController::onKeyboardBrightnessChanged(int value, const QString &source)
{
    qCDebug(POWERDEVIL) << "Keyboard brightness changed!!";
    if (value != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = value;
        // source: internal = keyboard brightness changed through hardware, eg a firmware-handled hotkey being pressed -> show the OSD
        //         external = keyboard brightness changed through upower -> don't trigger the OSD as we would already have done that where necessary
        m_keyboardBrightnessLogic.setValueMax(keyboardBrightnessMax());
        m_keyboardBrightnessLogic.setValue(value);

        if (source == QLatin1String("internal")) {
            BrightnessOSDWidget::show(m_keyboardBrightnessLogic.percentage(value), PowerDevil::BackendInterface::Keyboard);
        }

        Q_EMIT keyboardBrightnessChanged(m_keyboardBrightnessLogic.info());
    }
}

#include "moc_keyboardbrightnesscontroller.cpp"