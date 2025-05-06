/*
 *   SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>

#include <memory>

#include <KScreenDpms/Dpms>

namespace PowerDevil::BundledActions
{

class TurnOffKeyboardBacklight : public PowerDevil::Action
{
    Q_OBJECT

public:
    explicit TurnOffKeyboardBacklight(QObject *parent);

protected:
    void onWakeupFromIdle() override;
    void onIdleTimeout(std::chrono::milliseconds timeout) override;
    bool isSupported() override;

public:
    bool loadAction(const PowerDevil::ProfileSettings &settings) override;

private:
    void setDimmed(bool dimmed);

    void onDpmsModeChanged(KScreen::Dpms::Mode mode);

    int m_oldKeyboardBrightness = 0;
    bool m_dimmed = false;

    std::chrono::milliseconds m_dimDisplayIdleTimeout{-1};

    std::unique_ptr<KScreen::Dpms> m_dpms;
};

} // namespace PowerDevil::BundledActions
