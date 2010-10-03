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


#include "brightnesscontrol.h"
#include <KConfigGroup>
#include <powerdevilbackendinterface.h>
#include <KLocalizedString>

namespace PowerDevil {
namespace BundledActions {

BrightnessControl::BrightnessControl(QObject* parent)
    : Action(parent)
{

}

BrightnessControl::~BrightnessControl()
{

}

void BrightnessControl::onProfileUnload()
{
    //
}

void BrightnessControl::onWakeupFromIdle()
{
    //
}

void BrightnessControl::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void BrightnessControl::onProfileLoad()
{
    if (m_defaultValue > 0) {
        QVariantMap args;
        args["Value"] = QVariant::fromValue((float)m_defaultValue);
        trigger(args);
    }
}

void BrightnessControl::trigger(const QVariantMap& args)
{
    backend()->setBrightness(args["Value"].toFloat());
}

bool BrightnessControl::loadAction(const KConfigGroup& config)
{
    if (config.hasKey("value")) {
        m_defaultValue = config.readEntry<int>("value", 50);
    } else {
        m_defaultValue = -1;
    }

    return true;
}

}
}
