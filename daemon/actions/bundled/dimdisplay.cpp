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

DimDisplay::DimDisplay(QObject* parent) : Action(parent)
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
    if (backend()->screenBrightness() == 0) {
        //Some drivers report brightness == 0 when display is off because of DPMS
        //(especially Intel driver). Don't change brightness in this case, or
        //backlight won't switch on later.
        //Furthermore, we can't dim if brightness is 0 already.
        return;
    }

    if (msec == m_dimOnIdleTime) {
        setBrightnessHelper(0, 0);
    } else if (msec == (m_dimOnIdleTime * 3 / 4)) {
        const int newBrightness = qRound(m_oldScreenBrightness / 8.0);
        setBrightnessHelper(newBrightness, 0);
    } else if (msec == (m_dimOnIdleTime * 1 / 2)) {
        m_oldScreenBrightness = backend()->screenBrightness();
        m_oldKeyboardBrightness = backend()->keyboardBrightness();

        const int newBrightness = qRound(m_oldScreenBrightness / 2.0);
        setBrightnessHelper(newBrightness, 0);
    }

    m_dimmed = true;
}

void DimDisplay::onProfileLoad(const QString &/*previousProfile*/, const QString &/*newProfile*/)
{
    //
}

void DimDisplay::setBrightnessHelper(int screen, int keyboard, bool force)
{
    trigger({
        {QStringLiteral("_ScreenBrightness"), QVariant::fromValue(screen)},
        {QStringLiteral("_KeyboardBrightness"), QVariant::fromValue(keyboard)},
        {QStringLiteral("Explicit"), QVariant::fromValue(force)},
    });
}

void DimDisplay::triggerImpl(const QVariantMap &args)
{
    backend()->setScreenBrightness(args.value(QStringLiteral("_ScreenBrightness")).toInt());

    // don't manipulate keyboard brightness if it's already zero to prevent races with DPMS action
    if (m_oldKeyboardBrightness > 0) {
        backend()->setKeyboardBrightness(args.value(QStringLiteral("_KeyboardBrightness")).toInt());
    }
}

bool DimDisplay::isSupported()
{
    return backend()->screenBrightnessAvailable();
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
