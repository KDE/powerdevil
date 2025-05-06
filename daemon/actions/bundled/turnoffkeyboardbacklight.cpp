/*
 *   SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "turnoffkeyboardbacklight.h"

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilcore.h>

#include <QDebug>
#include <QTimer>

#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::TurnOffKeyboardBacklight, "powerdevilturnoffkeyboardbacklightaction.json")

using namespace Qt::Literals::StringLiterals;

namespace PowerDevil::BundledActions
{

TurnOffKeyboardBacklight::TurnOffKeyboardBacklight(QObject *parent)
    : Action(parent)
    , m_dpms(std::make_unique<KScreen::Dpms>())
{
    connect(m_dpms.get(), &KScreen::Dpms::modeChanged, this, &TurnOffKeyboardBacklight::onDpmsModeChanged);
}

void TurnOffKeyboardBacklight::onWakeupFromIdle()
{
    setDimmed(false);
}

void TurnOffKeyboardBacklight::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_ASSERT(timeout == m_dimDisplayIdleTimeout);
    setDimmed(true);
}

bool TurnOffKeyboardBacklight::isSupported()
{
    return core()->keyboardBrightnessController()->isSupported();
}

bool TurnOffKeyboardBacklight::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    if (!profileSettings.dimDisplayWhenIdle()) {
        if (!profileSettings.useProfileSpecificKeyboardBrightness()) {
            core()->keyboardBrightnessController()->setBrightness(m_oldKeyboardBrightness);
        }
        m_dimmed = false;
        return false;
    }

    if (m_dimmed && profileSettings.useProfileSpecificKeyboardBrightness()) {
        m_oldKeyboardBrightness = profileSettings.keyboardBrightness();
    }

    const int idleTimeout = profileSettings.dimDisplayIdleTimeoutSec();
    m_dimDisplayIdleTimeout = std::chrono::seconds{idleTimeout};

    qCDebug(POWERDEVIL) << "TurnOffKeyboardBacklight: registering idle timeout after" << m_dimDisplayIdleTimeout;
    registerIdleTimeout(m_dimDisplayIdleTimeout);
    return true;
}

void TurnOffKeyboardBacklight::setDimmed(bool dimmed)
{
    if (m_dimmed == dimmed) {
        return;
    }

    if (dimmed) {
        m_oldKeyboardBrightness = core()->keyboardBrightnessController()->brightness();
        core()->keyboardBrightnessController()->setBrightness(0);
    } else {
        // An active inhibition may not let us restore the brightness.
        // We should wait a bit screen to wake-up from sleep
        QTimer::singleShot(0, this, [this]() {
            core()->keyboardBrightnessController()->setBrightness(m_oldKeyboardBrightness);
        });
    }

    m_dimmed = dimmed;
}

void TurnOffKeyboardBacklight::onDpmsModeChanged(KScreen::Dpms::Mode mode)
{
    setDimmed(mode != KScreen::Dpms::On);
}

} // namespace PowerDevil::BundledActions

#include "turnoffkeyboardbacklight.moc"

#include "moc_turnoffkeyboardbacklight.cpp"
