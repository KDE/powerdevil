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

#include <QAction>

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
    // For the time being, let the (legacy) BrightnessControl action set a single brightness value
    // on profile load. Once we're able to store brightness values per display, we can implement
    // this here to replace BrightnessControl's implementation.
    Q_UNUSED(previousProfile)
    Q_UNUSED(newProfile)
}

void ScreenBrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool ScreenBrightnessControl::isSupported()
{
    return true;
}

bool ScreenBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)

    return false;
}

void ScreenBrightnessControl::actOnBrightnessKey(BrightnessLogic::StepAdjustmentAction action)
{
    core()->screenBrightnessController()->adjustBrightnessStep(action, u"(internal)"_s, u"brightness_key"_s, ScreenBrightnessController::ShowIndicator);
}
}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
