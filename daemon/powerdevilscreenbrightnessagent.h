/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilbrightnesslogic.h>

#include <controllers/screenbrightnesscontroller.h> // ScreenBrightnessController::IndicatorHint enum

#include <powerdevilcore_export.h>

#include <QObject>
#include <QStringList>

#include <QDBusContext>

namespace PowerDevil
{

class POWERDEVILCORE_EXPORT ScreenBrightnessAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(ScreenBrightnessAgent)

    Q_CLASSINFO("D-Bus Interface", "org.kde.ScreenBrightness")

public:
    enum class AdjustBrightnessRatioFlags : uint {
        SuppressIndicator = 0x1, /// Don't show brightness change indicators such as OSD, notification, etc.
    };

    enum class AdjustBrightnessStepAction : uint {
        Increase = 0,
        Decrease = 1,
        IncreaseSmall = 2,
        DecreaseSmall = 3,
    };

    enum class AdjustBrightnessStepFlags : uint {
        SuppressIndicator = 0x1, /// Don't show brightness change indicators such as OSD, notification, etc.
    };

    enum class SetBrightnessFlags : uint {
        SuppressIndicator = 0x1, /// Don't show brightness change indicators such as OSD, notification, etc.
    };

public:
    explicit ScreenBrightnessAgent(QObject *parent, ScreenBrightnessController *controller);

    // D-Bus export
    QStringList GetDisplayIds() const;
    QString GetLabel(const QString &displayId) const;
    bool GetIsInternal(const QString &displayId) const;
    int GetBrightness(const QString &displayId) const;
    int GetMaxBrightness(const QString &displayId) const;

public Q_SLOTS:
    // D-Bus export
    void AdjustBrightnessRatio(double delta, uint flags = 0x0);
    void AdjustBrightnessRatioWithContext(double delta, uint flags, const QString &sourceClientContext);
    void AdjustBrightnessStep(uint stepAction, uint flags = 0x0);
    void AdjustBrightnessStepWithContext(uint stepDelta, uint flags, const QString &sourceClientContext);
    void SetBrightness(const QString &displayId, int value, uint flags = 0x0);
    void SetBrightnessWithContext(const QString &displayId, int value, uint flags, const QString &sourceClientContext);

private Q_SLOTS:
    void onBrightnessChanged(const QString &displayId,
                             const BrightnessLogic::BrightnessInfo &brightnessInfo,
                             const QString &sourceClientName,
                             const QString &sourceClientContext,
                             ScreenBrightnessController::IndicatorHint hint);
    void onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &brightnessInfo);

Q_SIGNALS:
    // D-Bus export
    void DisplayAdded(const QString &displayId);
    void DisplayRemoved(const QString &displayId);
    void BrightnessChanged(const QString &displayId, int brightness, const QString &sourceClientName, const QString &sourceClientContext);
    void BrightnessRangeChanged(const QString &displayId, int maxBrightness, int brightness);

private:
    int brightnessPercent(double value, double max) const;

private:
    ScreenBrightnessController *m_controller;
};

}
