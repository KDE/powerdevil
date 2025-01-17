/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnesscontrol.h"

#include <controllers/screenbrightnesscontroller.h>
#include <powerdevilcore.h>

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>

#include <QDebug>

#include <KPluginFactory>
#include <Solid/Battery>
#include <Solid/Device>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::ScreenBrightnessControl, "powerdevilscreenbrightnesscontrolaction.json")

using namespace Qt::Literals::StringLiterals;

namespace PowerDevil::BundledActions
{
ScreenBrightnessControl::ScreenBrightnessControl(QObject *parent)
    : Action(parent)
{
    connect(core()->screenBrightnessController(), &ScreenBrightnessController::displayAdded, this, &ScreenBrightnessControl::displayAdded);

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for (const Solid::Device &device : devices) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery *>(device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply() && (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery)) {
            m_supportsBatteryProfiles = true;
        }
    }
}

void ScreenBrightnessControl::displayAdded(const QString &displayId)
{
    ScreenBrightnessController *controller = core()->screenBrightnessController();

    if (m_configuredBrightnessRatio.has_value() && controller->isInternal(displayId)) {
        const int newBrightness = std::round(*m_configuredBrightnessRatio * controller->maxBrightness(displayId));
        controller->setBrightness(displayId, newBrightness, u"(internal)"_s, u"profile_brightness"_s, ScreenBrightnessController::SuppressIndicator);
    }
}

void ScreenBrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    const bool isNewProfileMoreConservative = (newProfile == "Battery"_L1 && previousProfile == "AC"_L1)
        || (newProfile == "LowBattery"_L1 && (previousProfile == "AC"_L1 || previousProfile == "Battery"_L1));

    ScreenBrightnessController *controller = core()->screenBrightnessController();

    // Brightness changes based on power states make the most sense for internal displays,
    // as external ones are independently powered anyway. Limit to internal displays only,
    // both to avoid confusion with hotplugging display setup changes and to limit annoyances with
    // external monitors that are easily unplugged without being able to first restore brightness.
    for (const QString &displayId : controller->displayIds(DisplayFilter().isInternalEquals(true))) {
        const int newBrightness = std::round(*m_configuredBrightnessRatio * controller->maxBrightness(displayId));

        // don't change anything if the configuration would have us increase brightness when
        // switching to a more conservative profile
        if (isNewProfileMoreConservative && controller->brightness(displayId) < newBrightness) {
            qCDebug(POWERDEVIL) << "Display" << displayId << "not changing brightness: current brightness is lower and the new profile is more conservative";
        } else {
            controller->setBrightness(displayId, newBrightness, u"(internal)"_s, u"profile_brightness"_s, ScreenBrightnessController::SuppressIndicator);
        }
    }
}

void ScreenBrightnessControl::onProfileUnload()
{
    m_configuredBrightnessRatio = std::nullopt;
}

void ScreenBrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool ScreenBrightnessControl::isSupported()
{
    return m_supportsBatteryProfiles // users with only AC power should set brightness via applet or kcm_kscreen
        && core()->screenBrightnessController()->isSupported()
        && !core()->screenBrightnessController()->displayIds(DisplayFilter().isInternalEquals(true)).isEmpty();
}

bool ScreenBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    if (!profileSettings.useProfileSpecificDisplayBrightness()) {
        onProfileUnload();
        return false;
    }

    m_configuredBrightnessRatio = std::clamp(profileSettings.displayBrightness(), 0, 100) / 100.0;
    return true;
}
}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
