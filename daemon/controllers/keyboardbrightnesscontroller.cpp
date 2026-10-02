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

using namespace Qt::StringLiterals;

inline constexpr QLatin1StringView UPOWER_SERVICE("org.freedesktop.UPower");

KeyboardBrightnessController::KeyboardBrightnessController()
    : m_maxBrightness(0)
    , m_cachedBrightness(0)
    , m_kbdBacklight(nullptr)
{
    m_kbdBacklight =
        new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, u"/org/freedesktop/UPower/KbdBacklight"_s, QDBusConnection::systemBus(), this);
    connect(m_kbdBacklight, &OrgFreedesktopUPowerKbdBacklightInterface::BrightnessChangedWithSource, this, &KeyboardBrightnessController::onBrightnessChanged);

    QDBusConnection::systemBus().connect(UPOWER_SERVICE,
                                         u"/org/freedesktop/UPower"_s,
                                         u"org.freedesktop.UPower"_s,
                                         u"DeviceAdded"_s,
                                         this,
                                         SLOT(onUPowerDeviceChanged(QDBusObjectPath)));
    QDBusConnection::systemBus().connect(UPOWER_SERVICE,
                                         u"/org/freedesktop/UPower"_s,
                                         u"org.freedesktop.UPower"_s,
                                         u"DeviceRemoved"_s,
                                         this,
                                         SLOT(onUPowerDeviceChanged(QDBusObjectPath)));

    updateMaxBrightness();
}

void KeyboardBrightnessController::updateMaxBrightness()
{
    const bool wasSupported = m_isSupported;
    const int oldMaxBrightness = m_maxBrightness;

    QDBusPendingReply<int> rep = m_kbdBacklight->GetMaxBrightness();
    rep.waitForFinished();
    if (rep.isValid() && rep.value() > 0) {
        m_maxBrightness = rep.value();
        m_isSupported = true;
        m_cachedBrightness = brightness();
        qCDebug(POWERDEVIL) << "current keyboard backlight brightness value: " << m_cachedBrightness;
    } else {
        m_maxBrightness = 0;
        m_isSupported = false;
        // Don't warn when no keyboard backlight is available, only for other errors
        if (rep.isError() && rep.error().type() != QDBusError::UnknownMethod) {
            qCWarning(POWERDEVIL) << "Could not query keyboard backlight brightness" << rep.error().message();
        }
    }

    if (m_isSupported != wasSupported) {
        Q_EMIT supportedChanged();
    }
    if (m_isSupported && m_maxBrightness != oldMaxBrightness) {
        m_keyboardBrightnessLogic.setValueRange(0, m_maxBrightness);
        m_keyboardBrightnessLogic.setValue(m_cachedBrightness);
        Q_EMIT brightnessInfoChanged(m_keyboardBrightnessLogic.info());
    }
}

void KeyboardBrightnessController::onUPowerDeviceChanged(const QDBusObjectPath &path)
{
    if (path.path().startsWith(u"/org/freedesktop/UPower/KbdBacklight"_s)) {
        updateMaxBrightness();
    }
}

bool KeyboardBrightnessController::isSupported() const
{
    return m_isSupported;
}

int KeyboardBrightnessController::brightnessSteps()
{
    m_keyboardBrightnessLogic.setValueRange(0, maxBrightness());
    return m_keyboardBrightnessLogic.steps();
}

int KeyboardBrightnessController::calculateNextBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::StepAdjustmentAction adjustment)
{
    m_keyboardBrightnessLogic.setValueRange(0, valueMax);
    m_keyboardBrightnessLogic.setValue(value);

    return m_keyboardBrightnessLogic.adjusted(adjustment);
}

int KeyboardBrightnessController::brightness() const
{
    int result = m_kbdBacklight->GetBrightness();
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value: " << result;

    return result;
}

int KeyboardBrightnessController::maxBrightness() const
{
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value max: " << m_maxBrightness;
    return m_maxBrightness;
}

void KeyboardBrightnessController::setBrightness(int value)
{
    if (value == 0) {
        // save value before toggling so that we can restore it later
        m_brightnessBeforeTogglingOff = brightness();
    }
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << value;
    m_kbdBacklight->SetBrightness(value);
    if (value > 0) {
        m_brightnessBeforeTogglingOff = brightness();
    }
}

int KeyboardBrightnessController::keyboardBrightnessKeyPressed(PowerDevil::BrightnessLogic::StepAdjustmentAction adjustment)
{
    if (!m_isSupported) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = brightness();
    if (currentBrightness != m_cachedBrightness) {
        m_cachedBrightness = currentBrightness;
        return currentBrightness;
    }

    int newBrightness = calculateNextBrightnessStep(currentBrightness, maxBrightness(), adjustment);
    if (newBrightness < 0) {
        return -1;
    }

    setBrightness(newBrightness);
    return newBrightness;
}

int KeyboardBrightnessController::toggleBacklight()
{
    if (!m_isSupported) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = brightness();
    if (currentBrightness != m_cachedBrightness) {
        m_cachedBrightness = currentBrightness;
        return currentBrightness;
    }

    int newBrightness = 0;

    if (currentBrightness > 0) {
        newBrightness = 0; // currently on: toggle off
    } else if (m_brightnessBeforeTogglingOff > 0) {
        newBrightness = m_brightnessBeforeTogglingOff; // currently off and was on before toggling: restore
    } else {
        newBrightness = maxBrightness(); // currently off and would stay off if restoring: toggle to max
    }

    setBrightness(newBrightness);
    return newBrightness;
}

void KeyboardBrightnessController::onBrightnessChanged(int value, const QString &source)
{
    qCDebug(POWERDEVIL) << "Keyboard brightness changed!!";
    if (m_isSupported && value != m_cachedBrightness) {
        m_cachedBrightness = value;
        // source: internal = keyboard brightness changed through hardware, eg a firmware-handled hotkey being pressed -> show the OSD
        //         external = keyboard brightness changed through upower -> don't trigger the OSD as we would already have done that where necessary
        m_keyboardBrightnessLogic.setValueRange(0, maxBrightness());
        m_keyboardBrightnessLogic.setValue(value);

        if (source == QLatin1String("internal")) {
            BrightnessOSDWidget::show(m_keyboardBrightnessLogic.valueAsRatio() * 100.0, PowerDevil::BrightnessControlType::Keyboard);
        }

        Q_EMIT brightnessInfoChanged(m_keyboardBrightnessLogic.info());
    }
}

#include "moc_keyboardbrightnesscontroller.cpp"
