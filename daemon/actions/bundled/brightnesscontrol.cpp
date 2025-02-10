/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "brightnesscontrol.h"

#include <powerdevilcore.h>

#include <brightnesscontroladaptor.h>

#include <QAction>

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
    // profile-based brightness changes were moved to the ScreenBrightnessControl action
    // in order to be independent of the (deprecated) D-Bus interface getting exposed
    Q_UNUSED(previousProfile)
    Q_UNUSED(newProfile)
}

bool BrightnessControl::isSupported()
{
    return core()->screenBrightnessController()->isSupported();
}

bool BrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)
    return false;
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
