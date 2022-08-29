/***************************************************************************
 *   Copyright (C) 2012 by Michael Zanetti <mzanetti@kde.org>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "keyboardbrightnesscontrol.h"

#include "keyboardbrightnesscontroladaptor.h"

#include "brightnessosdwidget.h"

#include <powerdevilcore.h>
#include <powerdevil_debug.h>

#include <QAction>
#include <QDebug>

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusInterface>
#include <QDBusPendingCall>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::KeyboardBrightnessControl, "powerdevilkeyboardbrightnesscontrolaction.json")

namespace PowerDevil {
namespace BundledActions {

KeyboardBrightnessControl::KeyboardBrightnessControl(QObject* parent, const QVariantList &)
    : Action(parent)
{
    // DBus
    new KeyboardBrightnessControlAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    connect(core()->backend(), &PowerDevil::BackendInterface::brightnessChanged, this, &PowerDevil::BundledActions::KeyboardBrightnessControl::onBrightnessChangedFromBackend);

    KActionCollection* actionCollection = new KActionCollection( this );
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));
    KGlobalAccel *accel = KGlobalAccel::self();

    QAction *globalAction = actionCollection->addAction(QLatin1String("Increase Keyboard Brightness"));
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Keyboard Brightness"));
    accel->setGlobalShortcut(globalAction, Qt::Key_KeyboardBrightnessUp);
    connect(globalAction, &QAction::triggered, this, &KeyboardBrightnessControl::increaseKeyboardBrightness);

    globalAction = actionCollection->addAction(QLatin1String("Decrease Keyboard Brightness"));
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Keyboard Brightness"));
    accel->setGlobalShortcut(globalAction, Qt::Key_KeyboardBrightnessDown);
    connect(globalAction, &QAction::triggered, this, &KeyboardBrightnessControl::decreaseKeyboardBrightness);

    globalAction = actionCollection->addAction("Toggle Keyboard Backlight");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Toggle Keyboard Backlight"));
    accel->setGlobalShortcut(globalAction, Qt::Key_KeyboardLightOnOff);
    connect(globalAction, &QAction::triggered, this, &KeyboardBrightnessControl::toggleKeyboardBacklight);

    // My laptop sets the keyboard brightness to zero when I close the lid and it suspends
    // this makes sure the keyboard brightness is restored when we wake up :)
    connect(backend(), &PowerDevil::BackendInterface::resumeFromSuspend, this, [this] {
        if (m_lastKeyboardBrightness > -1) {
            setKeyboardBrightnessSilent((int)m_lastKeyboardBrightness);
        }
    });
}

void KeyboardBrightnessControl::onProfileUnload()
{
    //
}

void KeyboardBrightnessControl::onWakeupFromIdle()
{
    //
}

void KeyboardBrightnessControl::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void KeyboardBrightnessControl::onProfileLoad()
{
    const PerceivedBrightness absoluteKeyboardBrightnessValue = PerceivedBrightness(qRound((int)m_defaultValue / 100.0 * keyboardBrightnessMax()));

    // The core switches its profile after all actions have been updated, so here core()->currentProfile() is still the old one
    const auto previousProfile = core()->currentProfile();
    qCDebug(POWERDEVIL) << "Profiles: " << m_currentProfile << previousProfile;

    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((m_currentProfile == QLatin1String("Battery") && previousProfile == QLatin1String("AC")) ||
         (m_currentProfile == QLatin1String("LowBattery") && (previousProfile == QLatin1String("AC") || previousProfile == QLatin1String("Battery")))) &&
        absoluteKeyboardBrightnessValue > keyboardBrightness()) {

        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing keyboard brightness, the current one is lower and the profile is more conservative";
    } else if ((int)absoluteKeyboardBrightnessValue > 0) {
        QVariantMap args{
            {QStringLiteral("Value"), (int)absoluteKeyboardBrightnessValue}
        };

        // plugging in/out the AC is always explicit
        if ((m_currentProfile == QLatin1String("AC") && previousProfile != QLatin1String("AC")) ||
            (m_currentProfile != QLatin1String("AC") && previousProfile == QLatin1String("AC"))) {
            args["Explicit"] = true;
            args["Silent"] = true; // but we still don't want to show the OSD then
        }

        trigger(args);
    }
}

void KeyboardBrightnessControl::triggerImpl(const QVariantMap &args)
{
    PerceivedBrightness value(args.value(QStringLiteral("Value")).toInt());

    backend()->setBrightness(value, BackendInterface::Keyboard);

    if (args.value(QStringLiteral("Explicit")).toBool() && !args.value(QStringLiteral("Silent")).toBool()) {
        BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
    }
}

bool KeyboardBrightnessControl::isSupported()
{
    BackendInterface::BrightnessControlsList controls = backend()->brightnessControlsAvailable();
    if (controls.key(BackendInterface::Keyboard).isEmpty()) {
        return false;
    }

    return true;
}

bool KeyboardBrightnessControl::loadAction(const KConfigGroup& config)
{
    // Handle profile changes
    m_currentProfile = config.parent().name();

    if (config.hasKey("value")) {
        m_defaultValue = PerceivedBrightness(config.readEntry<int>("value", 50));
    }

    return true;
}

void KeyboardBrightnessControl::onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &info, BackendInterface::BrightnessControlType type)
{
    if (type == BackendInterface::Keyboard) {
        m_lastKeyboardBrightness = PerceivedBrightness(info.value, keyboardBrightnessMax());
        Q_EMIT keyboardBrightnessChanged((int)PerceivedBrightness(info.value, keyboardBrightnessMax()));
        Q_EMIT keyboardBrightnessMaxChanged(info.valueMax);
    }
}

void KeyboardBrightnessControl::increaseKeyboardBrightness()
{
    backend()->brightnessKeyPressed(BrightnessLogic::Increase, BackendInterface::Keyboard);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::decreaseKeyboardBrightness()
{
    backend()->brightnessKeyPressed(BrightnessLogic::Decrease, BackendInterface::Keyboard);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::toggleKeyboardBacklight()
{
    backend()->brightnessKeyPressed(BrightnessLogic::Toggle, BackendInterface::Keyboard);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

PerceivedBrightness KeyboardBrightnessControl::perceivedKeyboardBrightness() const
{
    return backend()->perceivedBrightness(BackendInterface::Keyboard);
}

int KeyboardBrightnessControl::keyboardBrightness() const
{
    return (int)backend()->perceivedBrightness(BackendInterface::Keyboard);
}

int KeyboardBrightnessControl::keyboardBrightnessMax() const
{
    return backend()->brightnessMax(BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::setKeyboardBrightness(int value)
{
    trigger({
        {QStringLiteral("Value"), value},
        {QStringLiteral("Explicit"), true}
    });
}

void KeyboardBrightnessControl::setKeyboardBrightnessSilent(int value)
{
    trigger({
        {QStringLiteral("Value"), value},
        {QStringLiteral("Explicit"), true},
        {QStringLiteral("Silent"), true}
    });
}

int KeyboardBrightnessControl::keyboardBrightnessSteps() const
{
    return (int)backend()->brightnessSteps(BackendInterface::Keyboard);
}

PerceivedBrightness KeyboardBrightnessControl::keyboardBrightnessPercent() const
{
    const float maxBrightness = keyboardBrightnessMax();
    if (maxBrightness <= 0) {
        return 0_pb;
    }

    return PerceivedBrightness(qRound((int)perceivedKeyboardBrightness() / maxBrightness * 100));
}

}
}
#include "keyboardbrightnesscontrol.moc"
