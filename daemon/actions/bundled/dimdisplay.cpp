/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dimdisplay.h"

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>

#include <QDebug>
#include <QTimer>

#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::DimDisplay, "powerdevildimdisplayaction.json")

namespace PowerDevil::BundledActions
{
DimDisplay::DimDisplay(QObject *parent)
    : Action(parent)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);
}

void DimDisplay::onWakeupFromIdle()
{
    if (!m_dimmed) {
        return;
    }
    // An active inhibition may not let us restore the brightness.
    // We should wait a bit screen to wake-up from sleep
    QTimer::singleShot(0, this, [this]() {
        setBrightnessHelper(m_oldScreenBrightness, m_oldKeyboardBrightness, true);
    });
    m_dimmed = false;
}

void DimDisplay::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_ASSERT(timeout == m_dimOnIdleTime);
    if (core()->screenBrightnessController()->screenBrightness() == 0) {
        // Some drivers report brightness == 0 when display is off because of DPMS
        //(especially Intel driver). Don't change brightness in this case, or
        // backlight won't switch on later.
        // Furthermore, we can't dim if brightness is 0 already.
        return;
    }


    m_oldScreenBrightness = core()->screenBrightnessController()->screenBrightness();
    m_oldKeyboardBrightness = core()->keyboardBrightnessController()->keyboardBrightness();


    // Dim brightness to 30% of the original. 30% is chosen arbitrarily based on
    // assumption that e.g. 50% may be too bright for returning user to notice that
    // the screen is going to go off, while 20% may be too dark to be able to read
    // something on the screen.
    const int newBrightness = qRound(m_oldScreenBrightness * 0.3);
    setBrightnessHelper(newBrightness, 0);

    m_dimmed = true;
}

void DimDisplay::setBrightnessHelper(int screen, int keyboard, bool force)
{
    trigger({
        {QStringLiteral("_ScreenBrightness"), QVariant::fromValue(screen)},
        {QStringLiteral("_KeyboardBrightness"), QVariant::fromValue(keyboard)},
        {QStringLiteral("Explicit"), QVariant::fromValue(force)},
    });
}

void DimDisplay::triggerImpl(const QVariantMap &args)
{
    core()->screenBrightnessController()->setScreenBrightness(args.value(QStringLiteral("_ScreenBrightness")).toInt());

    // don't manipulate keyboard brightness if it's already zero to prevent races with DPMS action
    if (m_oldKeyboardBrightness > 0) {
        core()->keyboardBrightnessController()->setKeyboardBrightness(args.value(QStringLiteral("_KeyboardBrightness")).toInt());
    }
}

bool DimDisplay::isSupported()
{
    return core()->screenBrightnessController()->screenBrightnessAvailable();
}

bool DimDisplay::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    qCDebug(POWERDEVIL);
    if (!profileSettings.dimDisplayWhenIdle()) {
        return false;
    }

    m_dimOnIdleTime = std::chrono::seconds(profileSettings.dimDisplayIdleTimeoutSec());
    qCDebug(POWERDEVIL) << "Loading timeouts with " << m_dimOnIdleTime.count();
    registerIdleTimeout(m_dimOnIdleTime);
    return true;
}

}

#include "dimdisplay.moc"

#include "moc_dimdisplay.cpp"
