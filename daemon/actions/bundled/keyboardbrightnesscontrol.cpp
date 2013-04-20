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

#include "brightnessosdwidget.h"
#include "keyboardbrightnesscontroladaptor.h"

#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>

#include <QtGui/QDesktopWidget>

#include <KAction>
#include <KActionCollection>
#include <KApplication>
#include <KConfigGroup>
#include <KDebug>
#include <KLocalizedString>

namespace PowerDevil {
namespace BundledActions {

KeyboardBrightnessControl::KeyboardBrightnessControl(QObject* parent)
    : Action(parent)
{
    // DBus
    new KeyboardBrightnessControlAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    connect(core()->backend(), SIGNAL(brightnessChanged(float,PowerDevil::BackendInterface::BrightnessControlType)),
            this, SLOT(onBrightnessChangedFromBackend(float,PowerDevil::BackendInterface::BrightnessControlType)));

    KActionCollection* actionCollection = new KActionCollection( this );

    KAction *globalAction = actionCollection->addAction("Increase Keyboard Brightness");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Keyboard Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_KeyboardBrightnessUp));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(increaseKeyboardBrightness()));

    globalAction = actionCollection->addAction("Decrease Keyboard Brightness");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Keyboard Brightness"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_KeyboardBrightnessDown));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(decreaseKeyboardBrightness()));

    globalAction = actionCollection->addAction("Toggle Keyboard Backlight");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Toggle Keyboard Backlight"));
    globalAction->setGlobalShortcut(KShortcut(Qt::Key_KeyboardLightOnOff));
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(toggleKeyboardBacklight()));
}

KeyboardBrightnessControl::~KeyboardBrightnessControl()
{
    if (!m_brightnessOSD.isNull()) {
        m_brightnessOSD.data()->deleteLater();
    }
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
    // This unparsable conditional block means: if the current profile is more
    // conservative than the previous one and the current brightness is lower
    // than the new profile
    if (((m_currentProfile == "Battery" && m_lastProfile == "AC") ||
         (m_currentProfile == "LowBattery" && (m_lastProfile == "AC" || m_lastProfile == "Battery"))) &&
        m_defaultValue > keyboardBrightness()) {
        // We don't want to change anything here
        kDebug() << "Not changing keyboard brightness, the current one is lower and the profile is more conservative";
    } else if (m_defaultValue > 0) {
        QVariantMap args;
        args["Value"] = QVariant::fromValue((float)m_defaultValue);
        trigger(args);
    }
}

void KeyboardBrightnessControl::triggerImpl(const QVariantMap& args)
{
    backend()->setBrightness(args["Value"].toFloat(), BackendInterface::Keyboard);
    if (args["Explicit"].toBool()) {
        showBrightnessOSD(args["Value"].toFloat());
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
    m_lastProfile = m_currentProfile;
    m_currentProfile = config.parent().name();

    kDebug() << "Profiles: " << m_currentProfile << m_lastProfile;

    if (config.hasKey("value")) {
        m_defaultValue = config.readEntry<int>("value", 50);
    } else {
        m_defaultValue = -1;
    }

    return true;
}

void KeyboardBrightnessControl::showBrightnessOSD(int brightness)
{
    // code adapted from KMix
    if (m_brightnessOSD.isNull()) {
        m_brightnessOSD = new BrightnessOSDWidget(BackendInterface::Keyboard);
    }

    m_brightnessOSD.data()->setCurrentBrightness(brightness);
    m_brightnessOSD.data()->show();
    m_brightnessOSD.data()->activateOSD(); //Enable the hide timer

    //Center the OSD
    QRect rect = KApplication::kApplication()->desktop()->screenGeometry(QCursor::pos());
    QSize size = m_brightnessOSD.data()->sizeHint();
    int posX = rect.x() + (rect.width() - size.width()) / 2;
    int posY = rect.y() + 4 * rect.height() / 5;
    m_brightnessOSD.data()->setGeometry(posX, posY, size.width(), size.height());
}

void KeyboardBrightnessControl::onBrightnessChangedFromBackend(float brightness, PowerDevil::BackendInterface::BrightnessControlType type)
{
    if (type == BackendInterface::Keyboard) {
        showBrightnessOSD(brightness);
        Q_EMIT keyboardBrightnessChanged(brightness);
    }
}

void KeyboardBrightnessControl::increaseKeyboardBrightness()
{
    backend()->brightnessKeyPressed(BackendInterface::Increase, BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::decreaseKeyboardBrightness()
{
    backend()->brightnessKeyPressed(BackendInterface::Decrease, BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::toggleKeyboardBacklight()
{
    backend()->brightnessKeyPressed(BackendInterface::Toggle, BackendInterface::Keyboard);
}

int KeyboardBrightnessControl::keyboardBrightness() const
{
    return backend()->brightness(BackendInterface::Keyboard);
}

void KeyboardBrightnessControl::setKeyboardBrightness(int percent)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<float>((float)percent);
    args["Explicit"] = true;
    trigger(args);
}

}
}
