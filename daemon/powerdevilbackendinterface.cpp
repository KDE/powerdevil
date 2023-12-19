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

void BackendInterface::setBackendIsReady()
{
    Q_EMIT backendReady();
}

void BackendInterface::onScreenBrightnessChanged(int value, int valueMax)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

int BackendInterface::calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    return m_screenBrightnessLogic.action(keyType);
}
}

#include "moc_powerdevilbackendinterface.cpp"
