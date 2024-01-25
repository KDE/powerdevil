/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */
#pragma once

#include <QObject>

class Display : public QObject
{
    Q_OBJECT

public:
    // Display();
    // ~Display();

    QString label() const;
    int brightness();
    int maxBrightness();
    void setBrightness(int value);
    bool supportsBrightness() const;

private:
    QString m_label;
    int m_brightness;
    int m_maxBrightness;
    bool m_supportsBrightness;
};
