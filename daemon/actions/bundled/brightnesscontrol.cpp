/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "brightnesscontrol.h"

#include <powerdevilcore.h>

#include <PowerDevilProfileSettings.h>
#include <brightnesscontroladaptor.h>
#include <powerdevil_debug.h>

#include <QAction>
#include <QDebug>

#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::BrightnessControl, "powerdevilbrightnesscontrolaction.json")

namespace PowerDevil::BundledActions
{
BrightnessControl::BrightnessControl(QObject *parent)
    : Action(parent)
{
    // DBus
    new BrightnessControlAdaptor(this);

    connect(core()->screenBrightnessController(),
            &ScreenBrightnessController::legacyBrightnessInfoChanged,
            this,
            &PowerDevil::BundledActions::BrightnessControl::onBrightnessChangedFromController);
}

void BrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    const int absoluteBrightnessValue = qRound(m_defaultValue / 100.0 * brightnessMax());

    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((newProfile == QLatin1String("Battery") && previousProfile == QLatin1String("AC"))
         || (newProfile == QLatin1String("LowBattery") && (previousProfile == QLatin1String("AC") || previousProfile == QLatin1String("Battery"))))
        && absoluteBrightnessValue > brightness()) {
        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing brightness, the current one is lower and the profile is more conservative";
    } else if (absoluteBrightnessValue >= 0) {
        core()->screenBrightnessController()->setBrightness(absoluteBrightnessValue);
    }
}

void BrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool BrightnessControl::isSupported()
{
    return core()->screenBrightnessController()->isSupported();
}

bool BrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    if (!profileSettings.useProfileSpecificDisplayBrightness()) {
        return false;
    }

    m_defaultValue = profileSettings.displayBrightness();
    return true;
}

void BrightnessControl::onBrightnessChangedFromController(const BrightnessLogic::BrightnessInfo &info, ScreenBrightnessController::IndicatorHint)
{
    if (info.value != m_lastBrightnessInfo.value) {
        Q_EMIT brightnessChanged(info.value);
    }
    if (info.valueMax != m_lastBrightnessInfo.valueMax) {
        Q_EMIT brightnessMaxChanged(info.valueMax);
    }
    if (info.valueMin != m_lastBrightnessInfo.valueMin) {
        Q_EMIT brightnessMinChanged(info.valueMin);
    }

    m_lastBrightnessInfo = info;
}

int BrightnessControl::brightness() const
{
    return core()->screenBrightnessController()->brightness();
}

int BrightnessControl::brightnessMax() const
{
    return core()->screenBrightnessController()->maxBrightness();
}

int BrightnessControl::brightnessMin() const
{
    return core()->screenBrightnessController()->minBrightness();
}

int BrightnessControl::knownSafeBrightnessMin() const
{
    // Theoretically we should provide a change signal for this property, because it can change as
    // display connections change, even if it's constant per individual display. In practice,
    // this won't change as long as laptop screens don't get added or removed at runtime.
    // Which shouldn't happen.
    return core()->screenBrightnessController()->knownSafeMinBrightness();
}

void BrightnessControl::setBrightness(int value)
{
    core()->screenBrightnessController()->setBrightness(value, ScreenBrightnessController::ShowIndicator);
}

void BrightnessControl::setBrightnessSilent(int value)
{
    core()->screenBrightnessController()->setBrightness(value, ScreenBrightnessController::SuppressIndicator);
}

void BrightnessControl::increaseBrightness()
{
    core()->screenBrightnessController()->adjustBrightnessStep(BrightnessLogic::Increase, QString(), QString(), ScreenBrightnessController::ShowIndicator);
}

void BrightnessControl::increaseBrightnessSmall()
{
    core()->screenBrightnessController()->adjustBrightnessStep(BrightnessLogic::IncreaseSmall, QString(), QString(), ScreenBrightnessController::ShowIndicator);
}

void BrightnessControl::decreaseBrightness()
{
    core()->screenBrightnessController()->adjustBrightnessStep(BrightnessLogic::Decrease, QString(), QString(), ScreenBrightnessController::ShowIndicator);
}

void BrightnessControl::decreaseBrightnessSmall()
{
    core()->screenBrightnessController()->adjustBrightnessStep(BrightnessLogic::DecreaseSmall, QString(), QString(), ScreenBrightnessController::ShowIndicator);
}

int BrightnessControl::brightnessSteps() const
{
    return core()->screenBrightnessController()->brightnessSteps();
}
}

#include "brightnesscontrol.moc"

#include "moc_brightnesscontrol.cpp"
