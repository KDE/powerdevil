/***************************************************************************
 *   Copyright (C) 2022 by Eric Edlund <ericedlund2017@gmail.com>          *
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

#include "brightnessvalue.h"

#include <cmath>
#include <QString>
#include <qmetatype.h>

PerceivedBrightness::PerceivedBrightness(const ActualBrightness &a, int maxBrightness)
{
    m_value =  std::round(std::sqrt((float)(int)a / maxBrightness) * maxBrightness);
}

ActualBrightness::ActualBrightness(const PerceivedBrightness &a, int maxBrightness)
{
    m_value = std::round(std::pow((float)(int)a / maxBrightness, 2.0) * maxBrightness);
}

PerceivedBrightness::PerceivedBrightness(int i)
{
    m_value = i;
}

PerceivedBrightness::operator int() const
{
    return m_value;
}

bool PerceivedBrightness::operator!=(const PerceivedBrightness& p) const
{
    return m_value != p.m_value;
}

bool PerceivedBrightness::operator==(const PerceivedBrightness& p) const
{
    return m_value == p.m_value;
}

PerceivedBrightness::operator QString() const
{
    return "Perceived: " + QString(m_value);
}

ActualBrightness::operator int() const
{
    return m_value;
}

ActualBrightness::ActualBrightness(int actual)
{
    m_value = actual;
}

bool ActualBrightness::operator!=(const ActualBrightness& a) const
{
    return m_value != a.m_value;
}

bool ActualBrightness::operator==(const ActualBrightness& a) const
{
    return m_value == a.m_value;
}

ActualBrightness::operator QString() const
{
    return "Real: " + QString(m_value);
}

bool PerceivedBrightness::operator>=(const PerceivedBrightness& p) const
{
    return m_value >= p.m_value;
}

bool PerceivedBrightness::operator<=(const PerceivedBrightness& p) const
{
    return m_value <= p.m_value;
}

bool PerceivedBrightness::operator<(const PerceivedBrightness& p) const
{
    return m_value < p.m_value;
}

bool PerceivedBrightness::operator>(const PerceivedBrightness& p) const
{
    return m_value > p.m_value;
}

PerceivedBrightness operator""_pb(unsigned long long val)
{
    return PerceivedBrightness((int)val);
}

