/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>

 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "backlightdisplay.h"

#include <KLocalizedString>

BacklightDisplay::BacklightDisplay()
    : m_cachedBrightness(-1)
    , m_maxBrightness(-1)
    , m_supportsBrightness(false)
{
}

BacklightDisplay::~BacklightDisplay()
{
}

QString BacklightDisplay::label() const
{
    return i18n("Built-in Screen");
}

int BacklightDisplay::brightness()
{
    return m_cachedBrightness;
}

int BacklightDisplay::maxBrightness()
{
    return m_maxBrightness;
}

void BacklightDisplay::setBrightness(int value)
{
    m_cachedBrightness = value;
}

bool BacklightDisplay::supportsBrightness() const
{
    return m_maxBrightness > 0;
}

#include "moc_backlightdisplay.cpp"
