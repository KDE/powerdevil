/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>
#include <powerdevilbrightnesslogic.h>

#include <controllers/screenbrightnesscontroller.h> // ScreenBrightnessController::IndicatorHint enum

namespace PowerDevil::BundledActions
{

// Legacy D-Bus API, operating on a single value instead of per-display.
// TODO KF7 (or earlier): remove this class and API, we have org.kde.ScreenBrightness instead
class BrightnessControl : public PowerDevil::Action
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.BrightnessControl")

public:
    explicit BrightnessControl(QObject *parent);

protected:
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const PowerDevil::ProfileSettings &profileSettings) override;

    int brightness() const;
    int brightnessMax() const;
    int brightnessMin() const;
    int knownSafeBrightnessMin() const;
    int brightnessSteps() const;

public Q_SLOTS:
    void increaseBrightness();
    void increaseBrightnessSmall();
    void decreaseBrightness();
    void decreaseBrightnessSmall();

    // DBus export
    void setBrightness(int percent);
    void setBrightnessSilent(int percent);

private Q_SLOTS:
    void onBrightnessChangedFromController(const BrightnessLogic::BrightnessInfo &brightnessInfo, ScreenBrightnessController::IndicatorHint hint);

Q_SIGNALS:
    void brightnessChanged(int value);
    void brightnessMaxChanged(int valueMax);
    void brightnessMinChanged(int valueMin);

private:
    BrightnessLogic::BrightnessInfo m_lastBrightnessInfo;
};

}
