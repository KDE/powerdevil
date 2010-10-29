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
#include <KConfigGroup>
#include <KLocalizedString>
#include <KDebug>

namespace PowerDevil {
namespace BundledActions {

DimDisplay::DimDisplay(QObject* parent)
    : Action(parent)
{

}

DimDisplay::~DimDisplay()
{

}

void DimDisplay::onProfileUnload()
{
}

void DimDisplay::onWakeupFromIdle()
{
    backend()->setBrightness(m_oldBrightness);
}

void DimDisplay::onIdleTimeout(int msec)
{
    if (msec == m_dimOnIdleTime) {
        backend()->setBrightness(0);
    } else if (msec == (m_dimOnIdleTime * 3 / 4)) {
        float newBrightness = backend()->brightness() / 4;
        backend()->setBrightness(newBrightness);
    } else if (msec == (m_dimOnIdleTime * 1 / 2)) {
        m_oldBrightness = backend()->brightness();
        float newBrightness = backend()->brightness() / 2;
        backend()->setBrightness(newBrightness);
    }
}

void DimDisplay::onProfileLoad()
{
    //
}

void DimDisplay::triggerImpl(const QVariantMap& args)
{
    Q_UNUSED(args);
}

bool DimDisplay::loadAction(const KConfigGroup& config)
{
    kDebug();
    if (config.hasKey("idleTime")) {
        m_dimOnIdleTime = config.readEntry<int>("idleTime", 10000000);
        kDebug() << "Loading timeouts with " << m_dimOnIdleTime;
        registerIdleTimeout(m_dimOnIdleTime * 3 / 4);
        registerIdleTimeout(m_dimOnIdleTime / 2);
        registerIdleTimeout(m_dimOnIdleTime);
    }

    return true;
}

}
}
