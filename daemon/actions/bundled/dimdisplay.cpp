/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dimdisplay.h"

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
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

    // Listen to the policy agent
    auto policyAgent = PowerDevil::PolicyAgent::instance();
    connect(policyAgent, &PowerDevil::PolicyAgent::unavailablePoliciesChanged, this, &DimDisplay::onUnavailablePoliciesChanged);

    m_inhibitScreen = policyAgent->unavailablePolicies() & PowerDevil::PolicyAgent::ChangeScreenSettings;
}

void DimDisplay::onWakeupFromIdle()
{
    if (!m_dimmed) {
        return;
    }
    // An active inhibition may not let us restore the brightness.
    // We should wait a bit screen to wake-up from sleep
    QTimer::singleShot(0, this, [this]() {
        qCDebug(POWERDEVIL) << "DimDisplay: restoring brightness on wake-up from idle";
        setBrightnessHelper(m_oldScreenBrightness, m_oldKeyboardBrightness);
        m_oldScreenBrightness.clear();
    });
    m_dimmed = false;
}

void DimDisplay::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_ASSERT(timeout == m_dimOnIdleTime);
    if (m_dimmed || m_inhibitScreen) {
        qCDebug(POWERDEVIL) << "DimDisplay: inhibited (or already dimmed), not dimming";
        return;
    }

    const QStringList displayIds = core()->screenBrightnessController()->displayIds();
    QMap<QString, int> oldScreenBrightness;
    QMap<QString, int> newScreenBrightness;

    for (const QString &displayId : displayIds) {
        const int oldBrightness = core()->screenBrightnessController()->brightness(displayId);

        if (oldBrightness <= 0) {
            // Some drivers report brightness == 0 when display is off because of DPMS
            //(especially Intel driver). Don't change brightness in this case, or
            // backlight won't switch on later.
            // Furthermore, we can't dim if brightness is 0 already.
            continue;
        }
        oldScreenBrightness[displayId] = oldBrightness;

        // Dim brightness to 30% of the original. 30% is chosen arbitrarily based on
        // assumption that e.g. 50% may be too bright for returning user to notice that
        // the screen is going to go off, while 20% may be too dark to be able to read
        // something on the screen.
        newScreenBrightness[displayId] = qRound(oldBrightness * 0.3);
    }

    if (!newScreenBrightness.isEmpty()) {
        qCDebug(POWERDEVIL) << "DimDisplay: triggered on idle timeout, dimming";
        m_oldScreenBrightness = oldScreenBrightness;
        m_oldKeyboardBrightness = core()->keyboardBrightnessController()->brightness();

        setBrightnessHelper(newScreenBrightness, 0);

        m_dimmed = true;
    }
}

void DimDisplay::setBrightnessHelper(const QMap<QString, int> &screenBrightness, int keyboardBrightness)
{
    // don't arbitrarily turn-off the display
    for (auto it = screenBrightness.keyValueBegin(); it != screenBrightness.keyValueEnd(); ++it) {
        const auto &[displayId, brightness] = *it;
        if (brightness > 0) {
            core()->screenBrightnessController()->setBrightness(displayId, brightness, QString(), QString(), ScreenBrightnessController::SuppressIndicator);
        }
    }

    // don't manipulate keyboard brightness if it's already zero to prevent races with DPMS action
    if (m_oldKeyboardBrightness > 0) {
        core()->keyboardBrightnessController()->setBrightness(keyboardBrightness);
    }
}

void DimDisplay::triggerImpl(const QVariantMap & /*args*/)
{
}

bool DimDisplay::isSupported()
{
    return core()->screenBrightnessController()->isSupported();
}

bool DimDisplay::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    // when profile is changed after display is dimmed or turn-off (by dpms)
    // 1. restore brightness values if current loaded profile doesn't require dimming
    // 2. update brightness values to new ones if there are explicitly set
    // but do not update m_dimmed so onIdleTimeout would not override them
    if (!profileSettings.dimDisplayWhenIdle()) {
        if (m_dimmed) {
            if (!profileSettings.useProfileSpecificDisplayBrightness() && !m_oldScreenBrightness.isEmpty()) {
                qCDebug(POWERDEVIL) << "DimDisplay: restoring brightness on reload";

                const QMap<QString, int> &oldScreenBrightness = std::as_const(m_oldScreenBrightness);
                for (auto it = oldScreenBrightness.keyValueBegin(); it != oldScreenBrightness.keyValueEnd(); ++it) {
                    const auto &[displayId, brightness] = *it;
                    core()->screenBrightnessController()->setBrightness(displayId,
                                                                        brightness,
                                                                        QString(),
                                                                        QString(),
                                                                        ScreenBrightnessController::SuppressIndicator);
                }
                m_oldScreenBrightness.clear();
            }
            if (!profileSettings.useProfileSpecificKeyboardBrightness() && m_oldKeyboardBrightness > 0) {
                core()->keyboardBrightnessController()->setBrightness(m_oldKeyboardBrightness);
            }
            m_dimmed = false;
        }
        return false;
    }

    if (m_dimmed) {
        if (profileSettings.useProfileSpecificDisplayBrightness()) {
            const double configuredBrightnessRatio = profileSettings.displayBrightness() / 100.0;

            // Mirror behavior of ScreenBrightnessControl::onProfileLoad(): Limit the setting
            // to internal displays if any are available, otherwise assign to all external ones.
            // TODO: Better centralize all this, see https://invent.kde.org/plasma/powerdevil/-/issues/38
            const ScreenBrightnessController *controller = core()->screenBrightnessController();
            const QStringList displayIds = core()->screenBrightnessController()->displayIds();
            bool hasInternal = false;
            QMap<QString, int> configuredScreenBrightness;

            for (const QString &displayId : displayIds) {
                const bool isDisplayInternal = controller->isInternal(displayId);
                if (hasInternal && !isDisplayInternal) {
                    continue;
                } else if (!hasInternal && isDisplayInternal) {
                    hasInternal = true;
                    configuredScreenBrightness.clear();
                }
                configuredScreenBrightness[displayId] = std::round(configuredBrightnessRatio * controller->maxBrightness(displayId));
            }
            m_oldScreenBrightness.insert(configuredScreenBrightness);
        }
        if (profileSettings.useProfileSpecificKeyboardBrightness()) {
            m_oldKeyboardBrightness = profileSettings.keyboardBrightness();
        }
    }

    m_dimOnIdleTime = std::chrono::seconds(profileSettings.dimDisplayIdleTimeoutSec());
    qCDebug(POWERDEVIL) << "DimDisplay: registering idle timeout after" << m_dimOnIdleTime;
    registerIdleTimeout(m_dimOnIdleTime);
    return true;
}

void DimDisplay::onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies)
{
    m_inhibitScreen = policies & PowerDevil::PolicyAgent::ChangeScreenSettings;
}
}

#include "dimdisplay.moc"

#include "moc_dimdisplay.cpp"
