/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QObject>

/**
 * Interface for accessing & manipulating brightness of a single display.
 */
class DisplayBrightness : public QObject
{
    Q_OBJECT

public:
    explicit DisplayBrightness(QObject *parent = nullptr);

    virtual int maxBrightness() const = 0;
    virtual int brightness() const = 0;
    virtual void setBrightness(int brightness) = 0;

Q_SIGNALS:
    void brightnessChanged(int brightness, int maxBrightness);
};
