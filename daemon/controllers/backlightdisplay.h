/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include "display.h"

#include <QObject>

class BacklightDisplay : public Display
{
    Q_OBJECT
public:
    BacklightDisplay();
    ~BacklightDisplay();

    QString label() const;
    int brightness();
    int maxBrightness();
    void setBrightness(int value);
    bool supportsBrightness() const;

private:
    int m_cachedBrightness;
    int m_maxBrightness;
    bool m_supportsBrightness;
};
