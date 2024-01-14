/*  This file is part of the KDE project
 *  SPDX-FileCopyrightText: 2010 Lukas Tinkl <ltinkl@redhat.com>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-only
 */

#pragma once

#include <QObject>
#include <QString>

#include "displaybrightness.h"

class QTimer;

namespace UdevQt
{
class Device;
}

class BacklightBrightness : public DisplayBrightness
{
    Q_OBJECT

public:
    explicit BacklightBrightness(QObject *parent = nullptr);

    /**
     * Check if this controller is operational. Call detect() and wait for the
     * detectionFinished() signal before using this.
     *
     * @return true after detectionFinished() is emitted, if a brightness-adjustable display is available.
     *         false otherwise.
     */
    bool isSupported() const;

    int maxBrightness() const override;
    int brightness() const override;
    void setBrightness(int brightness) override;

public Q_SLOTS:
    /**
     * Detect supported backlight devices. Emits detectionFinished() once completed.
     * Can be called repeatedly if needed.
     */
    void detect();

Q_SIGNALS:
    void detectionFinished(bool isSupported);

private Q_SLOTS:
    void onDeviceChanged(const UdevQt::Device &device);

private:
    QString m_syspath; // device path within sysfs

    const int m_brightnessAnimationThreshold = 100;
    const int m_brightnessAnimationDurationMsec = 250;
    QTimer *m_brightnessAnimationTimer = nullptr;

    int m_cachedBrightness = 0;
    int m_maxBrightness = 0;
};
