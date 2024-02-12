/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include <QObject>
#include <QStringList>

#include "ddcutildisplay.h"

class DDCutilBrightness : public QObject
{
    Q_OBJECT
public:
    explicit DDCutilBrightness(QObject *parent = nullptr);
    ~DDCutilBrightness();

    bool isSupported() const;
    int brightness(const QString &displayId);
    int maxBrightness(const QString &displayId);
    void setBrightness(const QString &displayId, int value);
    QStringList displayIds() const;
    void detect();
};
