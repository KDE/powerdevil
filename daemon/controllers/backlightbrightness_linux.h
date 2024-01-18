/*  This file is part of the KDE project
 *  SPDX-FileCopyrightText: 2010 Lukas Tinkl <ltinkl@redhat.com>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-only
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <memory> // std::unique_ptr

#include "backlightsysfsdevice.h"
#include "displaybrightness.h"

namespace UdevQt
{
class Device;
}

class BacklightBrightness;

class BacklightDetector : public DisplayBrightnessDetector
{
    Q_OBJECT

public:
    explicit BacklightDetector(QObject *parent = nullptr);

    void detect() override;
    QList<DisplayBrightness *> displays() const override;

private:
    std::unique_ptr<BacklightBrightness> m_display;
};

class BacklightBrightness : public DisplayBrightness
{
    Q_OBJECT

public:
    QString id() const override;
    QString label() const override;
    int knownSafeMinBrightness() const override;
    int maxBrightness() const override;
    int brightness() const override;
    void setBrightness(int brightness) override;
    bool isInternal() const override;

private Q_SLOTS:
    void onDeviceChanged(const UdevQt::Device &device);

private:
    friend class BacklightDetector;
    explicit BacklightBrightness(const QList<BacklightSysfsDevice> &devices, QObject *parent = nullptr);

    bool isSupported() const;
    void setBrightnessWithLogin1(int newBrightness);

private:
    QList<BacklightSysfsDevice> m_devices;

    int m_observedBrightness = -1;
    int m_requestedBrightness = -1;
    int m_executedBrightness = -1;

    // lower and upper bound for checking whether an observed brightness change is expected or not
    // by the brightness animation
    int m_expectedMinBrightness = -1;
    int m_expectedMaxBrightness = -1;

    bool m_isWaitingForAsyncIPC = false;
};
