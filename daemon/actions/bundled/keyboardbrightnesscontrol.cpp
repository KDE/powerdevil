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

KeyboardBrightnessControl::KeyboardBrightnessControl(QObject* parent)
    : Action(parent)
{
    // DBus
    new KeyboardBrightnessControlAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    connect(core()->backend(),
            &PowerDevil::BackendInterface::keyboardBrightnessChanged,
            this,
            &PowerDevil::BundledActions::KeyboardBrightnessControl::onBrightnessChangedFromBackend);

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
            setKeyboardBrightnessSilent(m_lastKeyboardBrightness);
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

void KeyboardBrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    const int absoluteKeyboardBrightnessValue = qRound(m_defaultValue / 100.0 * keyboardBrightnessMax());

    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((newProfile == QLatin1String("Battery") && previousProfile == QLatin1String("AC")) ||
         (newProfile == QLatin1String("LowBattery") && (previousProfile == QLatin1String("AC") || previousProfile == QLatin1String("Battery")))) &&
        absoluteKeyboardBrightnessValue > keyboardBrightness()) {

        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing keyboard brightness, the current one is lower and the profile is more conservative";
    } else if (absoluteKeyboardBrightnessValue > 0) {
        QVariantMap args{
            {QStringLiteral("Value"), QVariant::fromValue(absoluteKeyboardBrightnessValue)}
        };

        // plugging in/out the AC is always explicit
        if ((newProfile == QLatin1String("AC") && previousProfile != QLatin1String("AC")) ||
            (newProfile != QLatin1String("AC") && previousProfile == QLatin1String("AC"))) {
            args["Explicit"] = true;
            args["Silent"] = true; // but we still don't want to show the OSD then
        }

        trigger(args);
    }
}

void KeyboardBrightnessControl::triggerImpl(const QVariantMap &args)
{
    backend()->setKeyboardBrightness(args.value(QStringLiteral("Value")).toInt());

    if (args.value(QStringLiteral("Explicit")).toBool() && !args.value(QStringLiteral("Silent")).toBool()) {
        BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
    }
}

bool KeyboardBrightnessControl::isSupported()
{
    return backend()->keyboardBrightnessAvailable();
}

bool KeyboardBrightnessControl::loadAction(const KConfigGroup& config)
{
    if (config.hasKey("value")) {
        m_defaultValue = config.readEntry<int>("value", 50);
    }

    return true;
}

void KeyboardBrightnessControl::onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &info)
{
    m_lastKeyboardBrightness = info.value;
    Q_EMIT keyboardBrightnessChanged(info.value);
    Q_EMIT keyboardBrightnessMaxChanged(info.valueMax);
}

void KeyboardBrightnessControl::increaseKeyboardBrightness()
{
    backend()->keyboardBrightnessKeyPressed(BrightnessLogic::Increase);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::decreaseKeyboardBrightness()
{
    backend()->keyboardBrightnessKeyPressed(BrightnessLogic::Decrease);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::toggleKeyboardBacklight()
{
    backend()->keyboardBrightnessKeyPressed(BrightnessLogic::Toggle);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

int KeyboardBrightnessControl::keyboardBrightness() const
{
    return backend()->keyboardBrightness();
}

int KeyboardBrightnessControl::keyboardBrightnessMax() const
{
    return backend()->keyboardBrightnessMax();
}

void KeyboardBrightnessControl::setKeyboardBrightness(int percent)
{
    trigger({
        {QStringLiteral("Value"), QVariant::fromValue(percent)},
        {QStringLiteral("Explicit"), true},
    });
}

void KeyboardBrightnessControl::setKeyboardBrightnessSilent(int percent)
{
    trigger({
        {QStringLiteral("Value"), QVariant::fromValue(percent)},
        {QStringLiteral("Explicit"), true},
        {QStringLiteral("Silent"), true},
    });
}

int KeyboardBrightnessControl::keyboardBrightnessSteps() const
{
    return backend()->keyboardBrightnessSteps();
}

int KeyboardBrightnessControl::keyboardBrightnessPercent() const
{
    const float maxBrightness = keyboardBrightnessMax();
    if (maxBrightness <= 0) {
        return 0;
    }

    return qRound(keyboardBrightness() / maxBrightness * 100);
}

}
}
#include "keyboardbrightnesscontrol.moc"
