/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

namespace PowerDevil::BundledActions
{
class BrightnessControl : public PowerDevil::Action
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.BrightnessControl")

public:
    explicit BrightnessControl(QObject *parent);

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup &config) override;

    int brightness() const;
    int brightnessMax() const;
    int brightnessSteps() const;

public Q_SLOTS:
    // DBus export
    void increaseBrightness();
    void increaseBrightnessSmall();
    void decreaseBrightness();
    void decreaseBrightnessSmall();
    void setBrightness(int percent);
    void setBrightnessSilent(int percent);

private Q_SLOTS:
    void onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &brightnessInfo);

Q_SIGNALS:
    void brightnessChanged(int value);
    void brightnessMaxChanged(int valueMax);

private:
    int brightnessPercent(float value) const;

    int m_defaultValue = -1;
};

}
