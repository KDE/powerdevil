/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include "abstractlightsensor.h"

using namespace PowerDevil::BundledActions;
AbstractLightSensor::AbstractLightSensor(QObject *parent)
    : QObject(parent)
{
}

AbstractLightSensor::~AbstractLightSensor()
{
}

bool AbstractLightSensor::isValid() const
{
    return m_isValid.value();
}

bool AbstractLightSensor::enabled() const
{
    return m_enabled.value();
}

QBindable<bool> AbstractLightSensor::enabled()
{
    return &m_enabled;
}

double AbstractLightSensor::lightLevel() const
{
    return m_lightLevel;
}

#include "moc_abstractlightsensor.cpp"