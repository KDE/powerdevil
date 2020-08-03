/*
 * Copyright 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "turnoffkeyboardbacklight.h"

#include <powerdevilbackendinterface.h>
#include <powerdevil_debug.h>

#include <KConfigGroup>

using namespace PowerDevil::BundledActions;

TurnOffKeyboardBacklight::TurnOffKeyboardBacklight(QObject *parent)
    : Action(parent)
{

}

TurnOffKeyboardBacklight::~TurnOffKeyboardBacklight() = default;

void TurnOffKeyboardBacklight::onProfileLoad()
{

}

void TurnOffKeyboardBacklight::onProfileUnload()
{

}

void TurnOffKeyboardBacklight::onWakeupFromIdle()
{
    if (m_oldKeyboardBrightness > -1) {
        backend()->setBrightness(m_oldKeyboardBrightness, BackendInterface::Keyboard);
        m_oldKeyboardBrightness = -1;
    }
}

void TurnOffKeyboardBacklight::onIdleTimeout(int msec)
{
    if (msec != m_turnOffIdleTime) {
        return;
    }

    const int keyboardBrightness = backend()->brightness(BackendInterface::Keyboard);
    if (keyboardBrightness > 0) {
        m_oldKeyboardBrightness = keyboardBrightness;
        backend()->setBrightness(0, BackendInterface::Keyboard);
    }
}

void TurnOffKeyboardBacklight::triggerImpl(const QVariantMap &args)
{
    Q_UNUSED(args)
}

bool TurnOffKeyboardBacklight::isSupported()
{
    const auto controls = backend()->brightnessControlsAvailable();
    return !controls.key(BackendInterface::Keyboard).isEmpty();
}

bool TurnOffKeyboardBacklight::loadAction(const KConfigGroup &config)
{
    if (config.hasKey("idleTime")) {
        m_turnOffIdleTime = config.readEntry<int>("idleTime", 300000);
        registerIdleTimeout(m_turnOffIdleTime);
    }

    return true;
}
