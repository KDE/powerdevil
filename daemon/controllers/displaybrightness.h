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

    /**
     * The minimum practical brightness value that still results in contents being visible.
     *
     * Some screens fully turn off when set to 0. Brightness adjustments should not be used to
     * turn screens off, use DPMS interfaces for this instead. Setting brightness to values in
     * the range [0, knownSafeMinBrightness()) might still keep contents visible, but should
     * only be done with informed user consent.
     */
    virtual int knownSafeMinBrightness() const = 0;

    virtual int maxBrightness() const = 0;
    virtual int brightness() const = 0;
    virtual void setBrightness(int brightness) = 0;

Q_SIGNALS:
    void brightnessChanged(int brightness, int maxBrightness);
};

/**
 * Interface for accessing & manipulating brightness of a single screen.
 */
class DisplayBrightnessDetector : public QObject
{
    Q_OBJECT

public:
    explicit DisplayBrightnessDetector(QObject *parent = nullptr);

    /**
     * Detect displays that support brightness operations. Emits detectionFinished() once completed.
     * Can be called repeatedly if needed.
     */
    virtual void detect() = 0;

    /**
     * Retrieve a list of displays that support brightness operations.
     * Call detect() and wait for the detectionFinished() signal before using this.
     *
     * @return true after detectionFinished() is emitted, if a brightness-adjustable display is available.
     *         false otherwise.
     */
    virtual QList<DisplayBrightness *> displays() const = 0;

Q_SIGNALS:
    void detectionFinished(bool isSupported);
    void displaysChanged();
};
