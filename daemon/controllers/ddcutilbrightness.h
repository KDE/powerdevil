/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include <QList>
#include <QObject>

class DisplayBrightness;

class DDCutilBrightness : public QObject
{
    Q_OBJECT

public:
    explicit DDCutilBrightness(QObject *parent = nullptr);
    ~DDCutilBrightness();

    void detect();
    bool isSupported() const;
    QList<DisplayBrightness *> displays() const;

Q_SIGNALS:
    void displaysChanged();
};
