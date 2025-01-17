/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>

namespace PowerDevil::BundledActions
{

class ScreenBrightnessControl : public PowerDevil::Action, protected QDBusContext
{
    Q_OBJECT

public:
    explicit ScreenBrightnessControl(QObject *parent);

    bool loadAction(const PowerDevil::ProfileSettings &profileSettings) override;

protected:
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void onProfileUnload() override;
    bool isSupported() override;

private Q_SLOTS:
    void displayAdded(const QString &displayId);

private:
    std::optional<double> m_configuredBrightnessRatio;
    bool m_supportsBatteryProfiles = false;
};
}
