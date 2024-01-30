/*
 *   SPDX-FileCopyrightText: 2012 Michael Zanetti <mzanetti@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "keyboardbrightnesscontrol.h"

#include "keyboardbrightnesscontroladaptor.h"

#include "brightnessosdwidget.h"

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilcore.h>

#include <QAction>
#include <QDebug>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusInterface>
#include <QDBusPendingCall>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::KeyboardBrightnessControl, "powerdevilkeyboardbrightnesscontrolaction.json")

namespace PowerDevil::BundledActions
{
KeyboardBrightnessControl::KeyboardBrightnessControl(QObject *parent)
    : Action(parent)
{
    // DBus
    new KeyboardBrightnessControlAdaptor(this);

    connect(core()->backend(),
            &PowerDevil::BackendInterface::keyboardBrightnessChanged,
            this,
            &PowerDevil::BundledActions::KeyboardBrightnessControl::onBrightnessChangedFromBackend);

    KActionCollection *actionCollection = new KActionCollection(this);
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
    connect(core()->suspendController(), &SuspendController::resumeFromSuspend, this, [this] {
        if (m_lastKeyboardBrightness > 0) {
            setKeyboardBrightnessSilent(m_lastKeyboardBrightness);
        }
    });
}

void KeyboardBrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    const int absoluteKeyboardBrightnessValue = qRound(m_defaultValue / 100.0 * keyboardBrightnessMax());

    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((newProfile == QLatin1String("Battery") && previousProfile == QLatin1String("AC"))
         || (newProfile == QLatin1String("LowBattery") && (previousProfile == QLatin1String("AC") || previousProfile == QLatin1String("Battery"))))
        && absoluteKeyboardBrightnessValue > keyboardBrightness()) {
        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing keyboard brightness, the current one is lower and the profile is more conservative";
    } else {
        backend()->setKeyboardBrightness(absoluteKeyboardBrightnessValue);
    }
}

void KeyboardBrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool KeyboardBrightnessControl::isSupported()
{
    return backend()->keyboardBrightnessAvailable();
}

bool KeyboardBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    if (!profileSettings.useProfileSpecificKeyboardBrightness()) {
        return false;
    }

    m_defaultValue = profileSettings.keyboardBrightness();
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
    backend()->setKeyboardBrightness(percent);
    BrightnessOSDWidget::show(keyboardBrightnessPercent(), BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::setKeyboardBrightnessSilent(int percent)
{
    backend()->setKeyboardBrightness(percent);
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

#include "keyboardbrightnesscontrol.moc"

#include "moc_keyboardbrightnesscontrol.cpp"
