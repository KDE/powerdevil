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

#include <KConfigGroup>
#include <KLocalizedString>

namespace PowerDevil {
namespace BundledActions {

DimDisplay::DimDisplay(QObject* parent)
    : Action(parent), m_dimmed(false)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);
}

DimDisplay::~DimDisplay()
{

}

void DimDisplay::onProfileUnload()
{
}

void DimDisplay::onWakeupFromIdle()
{
    if (!m_dimmed) {
        return;
    }
    setBrightnessHelper(m_oldBrightness);
    m_dimmed = false;
}

void DimDisplay::onIdleTimeout(int msec)
{
    if (qFuzzyIsNull(backend()->brightness())) {
        //Some drivers report brightness == 0 when display is off because of DPMS
        //(especially Intel driver). Don't change brightness in this case, or
        //backlight won't switch on later.
        //Furthermore, we can't dim if brightness is 0 already.
        return;
    }

    if (msec == m_dimOnIdleTime) {
        setBrightnessHelper(0);
    } else if (msec == (m_dimOnIdleTime * 3 / 4)) {
        float newBrightness = backend()->brightness() / 4;
        setBrightnessHelper(newBrightness);
    } else if (msec == (m_dimOnIdleTime * 1 / 2)) {
        m_oldBrightness = backend()->brightness();
        float newBrightness = backend()->brightness() / 2;
        setBrightnessHelper(newBrightness);
    }

    m_dimmed = true;
}

void DimDisplay::onProfileLoad()
{
    //
}

void DimDisplay::setBrightnessHelper(float brightness)
{
    QVariantMap args;
    args["_BrightnessValue"] = QVariant::fromValue(brightness);
    trigger(args);
}

void DimDisplay::triggerImpl(const QVariantMap& args)
{
    backend()->setBrightness(args["_BrightnessValue"].toFloat());
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
