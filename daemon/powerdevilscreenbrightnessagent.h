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

class ScreenBrightnessDisplay : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(ScreenBrightnessDisplay)

    Q_CLASSINFO("D-Bus Interface", "org.kde.ScreenBrightness.Display")

    Q_PROPERTY(QString DBusName READ DBusName CONSTANT)
    Q_PROPERTY(QString Label READ Label)
    Q_PROPERTY(bool IsInternal READ IsInternal CONSTANT)
    Q_PROPERTY(int Brightness READ Brightness)
    Q_PROPERTY(int MaxBrightness READ MaxBrightness)

public:
    enum class SetBrightnessFlags : uint {
        SuppressIndicator = 0x1, /// Don't show brightness change indicators such as OSD, notification, etc.
    };

public:
    explicit ScreenBrightnessDisplay(QObject *parent, const QString &dbusName, ScreenBrightnessController *controller, const QString &displayId);

    ~ScreenBrightnessDisplay();

    // D-Bus object path elements can only contain "[A-Z][a-z][0-9]_", so we can't use
    // the display ID as D-Bus name. Store it just for internal lookup purposes.
    QString displayId() const;
    QString dbusPath() const;

    // D-Bus export
    QString DBusName() const;
    QString Label() const;
    bool IsInternal() const;
    int Brightness() const;
    int MaxBrightness() const;

    void SetBrightness(int value, uint flags = 0x0);
    void SetBrightnessWithContext(int value, uint flags, const QString &sourceClientContext);

private:
    QString m_dbusName;
    QString m_displayId;
    ScreenBrightnessController *m_controller;
};

class POWERDEVILCORE_EXPORT ScreenBrightnessAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(ScreenBrightnessAgent)

    Q_CLASSINFO("D-Bus Interface", "org.kde.ScreenBrightness")

    Q_PROPERTY(QStringList DisplaysDBusNames READ DisplaysDBusNames)

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

public:
    explicit ScreenBrightnessAgent(QObject *parent, ScreenBrightnessController *controller);

    // D-Bus property
    QStringList DisplaysDBusNames() const;

public Q_SLOTS:
    // D-Bus export
    void AdjustBrightnessRatio(double delta, uint flags = 0x0);
    void AdjustBrightnessRatioWithContext(double delta, uint flags, const QString &sourceClientContext);
    void AdjustBrightnessStep(uint stepAction, uint flags = 0x0);
    void AdjustBrightnessStepWithContext(uint stepDelta, uint flags, const QString &sourceClientContext);

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
    QString insertDisplayChild(const QString &displayId);
    int brightnessPercent(double value, double max) const;

private:
    std::unordered_map<QString, std::unique_ptr<ScreenBrightnessDisplay>> m_displayChildren;
    size_t m_nextDbusDisplayIndex = 0;
    ScreenBrightnessController *m_controller;
};

}
