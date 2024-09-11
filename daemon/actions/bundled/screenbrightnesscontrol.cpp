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

#include <QAction>
#include <QDebug>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::ScreenBrightnessControl, "powerdevilscreenbrightnesscontrolaction.json")

using namespace Qt::Literals::StringLiterals;

namespace PowerDevil::BundledActions
{
ScreenBrightnessControl::ScreenBrightnessControl(QObject *parent)
    : Action(parent)
{
    KActionCollection *actionCollection = new KActionCollection(this);
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));

    QAction *globalAction = actionCollection->addAction("Increase Screen Brightness"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessUp);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::Increase);
    });

    globalAction = actionCollection->addAction("Increase Screen Brightness Small"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness by 1%"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::ShiftModifier | Qt::Key_MonBrightnessUp);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::IncreaseSmall);
    });

    globalAction = actionCollection->addAction("Decrease Screen Brightness"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessDown);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::Decrease);
    });

    globalAction = actionCollection->addAction("Decrease Screen Brightness Small"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness by 1%"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::ShiftModifier | Qt::Key_MonBrightnessDown);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::DecreaseSmall);
    });
}

void ScreenBrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    const bool isNewProfileMoreConservative = (newProfile == "Battery"_L1 && previousProfile == "AC"_L1)
        || (newProfile == "LowBattery"_L1 && (previousProfile == "AC"_L1 || previousProfile == "Battery"_L1));

    ScreenBrightnessController *controller = core()->screenBrightnessController();
    QList<QString> displayIds = controller->displayIds();

    bool hasInternal;
    QList<bool> isInternal;
    isInternal.reserve(displayIds.size());

    for (const QString &displayId : std::as_const(displayIds)) {
        bool isDisplayInternal = controller->isInternal(displayId);
        isInternal.append(isDisplayInternal);
        hasInternal |= isDisplayInternal;
    }

    for (int i = 0; i < displayIds.size(); ++i) {
        // Brightness changes based on power states makes the most sense for internal displays,
        // as external ones are independently powered anyway. Limit this to internal displays if
        // any are available. Only change brightness for external displays if the setting was
        // configured anyway. This also limits annoyances for brightness changed to a different
        // value but not restored prior to unplugging, especially as long as pre-dimming brightness
        // is not persisted and restored for each display.
        if (isInternal[i] != hasInternal) {
            continue;
        }
        const auto &displayId = displayIds.at(i);
        const int newBrightness = std::round(m_configuredBrightnessRatio * controller->maxBrightness(displayId));

        // don't change anything if the configuration would have us increase brightness when
        // switching to a more conservative profile
        if (isNewProfileMoreConservative && controller->brightness(displayId) < newBrightness) {
            qCDebug(POWERDEVIL) << "Display" << displayId << "not changing brightness: current brightness is lower and the new profile is more conservative";
        } else if (newBrightness >= controller->minBrightness(displayId)) {
            controller->setBrightness(displayId, newBrightness, QString(), QString(), ScreenBrightnessController::SuppressIndicator);
        }
    }
}

void ScreenBrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool ScreenBrightnessControl::isSupported()
{
    return core()->screenBrightnessController()->isSupported();
}

bool ScreenBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    if (!profileSettings.useProfileSpecificDisplayBrightness()) {
        return false;
    }

    m_configuredBrightnessRatio = profileSettings.displayBrightness() / 100.0;
    return true;
}

void ScreenBrightnessControl::actOnBrightnessKey(BrightnessLogic::StepAdjustmentAction action)
{
    core()->screenBrightnessController()->adjustBrightnessStep(action, u"(internal)"_s, u"brightness_key"_s, ScreenBrightnessController::ShowIndicator);
}
}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
