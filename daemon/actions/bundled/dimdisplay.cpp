/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#include "dimdisplay.h"

#include <powerdevilbackendinterface.h>
#include <powerdevil_debug.h>

#include <QDebug>
#include <QTimer>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::DimDisplay, "powerdevildimdisplayaction.json")

namespace PowerDevil {
namespace BundledActions {

DimDisplay::DimDisplay(QObject* parent, const QVariantList &) : Action(parent)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);
}

void DimDisplay::onProfileUnload()
{

}

void DimDisplay::onWakeupFromIdle()
{
    if (!m_dimmed) {
        return;
    }
    // An active inhibition may not let us restore the brightness.
    // We should wait a bit screen to wake-up from sleep
    QTimer::singleShot(0, this, [this]() {
        setBrightnessHelper(m_oldScreenBrightness, m_oldKeyboardBrightness, true);
    });
    m_dimmed = false;
}

void DimDisplay::onIdleTimeout(int msec)
{
    if (backend()->brightness(BackendInterface::Screen) == ActualBrightness(0)) {
        //Some drivers report brightness == 0 when display is off because of DPMS
        //(especially Intel driver). Don't change brightness in this case, or
        //backlight won't switch on later.
        //Furthermore, we can't dim if brightness is 0 already.
        return;
    }

    if (msec == m_dimOnIdleTime) {
        setBrightnessHelper(0_pb, 0_pb);
    } else if (msec == (m_dimOnIdleTime * 3 / 4)) {
        const PerceivedBrightness newBrightness(qRound((int)m_oldScreenBrightness / 8.0));
        setBrightnessHelper(newBrightness, 0_pb);
    } else if (msec == (m_dimOnIdleTime * 1 / 2)) {
        m_oldScreenBrightness = PerceivedBrightness(backend()->brightness(BackendInterface::Screen), backend()->brightnessMax(BackendInterface::Screen));
        m_oldKeyboardBrightness = PerceivedBrightness(backend()->brightness(BackendInterface::Keyboard), backend()->brightnessMax(BackendInterface::Keyboard));

        const PerceivedBrightness newBrightness(qRound((int)m_oldScreenBrightness / 2.0));
        setBrightnessHelper(newBrightness, 0_pb);
    }

    m_dimmed = true;
}

void DimDisplay::onProfileLoad()
{
    //
}

void DimDisplay::setBrightnessHelper(PerceivedBrightness screen, PerceivedBrightness keyboard, bool force)
{
    trigger({
        {QStringLiteral("_ScreenBrightness"), QVariant::fromValue(screen)},
        {QStringLiteral("_KeyboardBrightness"), QVariant::fromValue(keyboard)},
        {QStringLiteral("Explicit"), QVariant::fromValue(force)}
    });
}

void DimDisplay::triggerImpl(const QVariantMap &args)
{
    backend()->setBrightness(ActualBrightness(args.value(QStringLiteral("_ScreenBrightness")).value<PerceivedBrightness>(), backend()->brightnessMax(BackendInterface::Screen)), BackendInterface::Screen);

    // don't manipulate keyboard brightness if it's already zero to prevent races with DPMS action
    if ((int)m_oldKeyboardBrightness > 0) {
        backend()->setBrightness(ActualBrightness(args.value(QStringLiteral("_KeyboardBrightness")).value<PerceivedBrightness>(), backend()->brightnessMax(BackendInterface::Keyboard)), BackendInterface::Keyboard);
    }
}

bool DimDisplay::isSupported()
{
    BackendInterface::BrightnessControlsList controls = backend()->brightnessControlsAvailable();
    if (controls.key(BackendInterface::Screen).isEmpty()) {
        return false;
    }

    return true;
}

bool DimDisplay::loadAction(const KConfigGroup& config)
{
    qCDebug(POWERDEVIL);
    if (config.hasKey("idleTime")) {
        m_dimOnIdleTime = config.readEntry<int>("idleTime", 10000000);
        qCDebug(POWERDEVIL) << "Loading timeouts with " << m_dimOnIdleTime;
        registerIdleTimeout(m_dimOnIdleTime * 3 / 4);
        registerIdleTimeout(m_dimOnIdleTime / 2);
        registerIdleTimeout(m_dimOnIdleTime);
    }

    return true;
}

}
}

#include "dimdisplay.moc"
