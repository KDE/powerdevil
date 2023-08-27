/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilbackendinterface.h"
#include "brightnessosdwidget.h"
#include "powerdevil_debug.h"
#include <QDebug>

namespace PowerDevil
{

BackendInterface::BackendInterface(QObject *parent)
    : QObject(parent)
{
}

BackendInterface::~BackendInterface()
{
}

int BackendInterface::screenBrightnessSteps()
{
    m_screenBrightnessLogic.setValueMax(screenBrightnessMax());
    return m_screenBrightnessLogic.steps();
}

int BackendInterface::keyboardBrightnessSteps()
{
    m_keyboardBrightnessLogic.setValueMax(keyboardBrightnessMax());
    return m_keyboardBrightnessLogic.steps();
}

bool BackendInterface::isLidClosed() const
{
    return m_isLidClosed;
}

bool BackendInterface::isLidPresent() const
{
    return m_isLidPresent;
}

void BackendInterface::setLidPresent(bool present)
{
    m_isLidPresent = present;
}

void BackendInterface::setBackendIsReady()
{
    Q_EMIT backendReady();
}

void BackendInterface::setButtonPressed(PowerDevil::BackendInterface::ButtonType type)
{
    if (type == LidClose && !m_isLidClosed) {
        m_isLidClosed = true;
        Q_EMIT lidClosedChanged(true);
    } else if (type == LidOpen && m_isLidClosed) {
        m_isLidClosed = false;
        Q_EMIT lidClosedChanged(false);
    }
    Q_EMIT buttonPressed(type);
}

void BackendInterface::onScreenBrightnessChanged(int value, int valueMax)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

void BackendInterface::onKeyboardBrightnessChanged(int value, int valueMax, bool notify)
{
    m_keyboardBrightnessLogic.setValueMax(valueMax);
    m_keyboardBrightnessLogic.setValue(value);

    if (notify) {
        BrightnessOSDWidget::show(m_keyboardBrightnessLogic.percentage(value), PowerDevil::BackendInterface::Keyboard);
    }

    Q_EMIT keyboardBrightnessChanged(m_keyboardBrightnessLogic.info());
}

int BackendInterface::calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    return m_screenBrightnessLogic.action(keyType);
}

int BackendInterface::calculateNextKeyboardBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    m_keyboardBrightnessLogic.setValueMax(valueMax);
    m_keyboardBrightnessLogic.setValue(value);
    m_keyboardBrightnessLogic.setValueBeforeTogglingOff(m_keyboardBrightnessBeforeTogglingOff);

    return m_keyboardBrightnessLogic.action(keyType);
}
}

#include "moc_powerdevilbackendinterface.cpp"
