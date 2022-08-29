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

#pragma once

#include <cmath>
#include <QString>
#include <qmetatype.h>

/**
 * These two classes are int wrappers used for brightness values.
 * This is a typesafe, dummy proof way of not confusing Perceived
 * and Actual brightness values.
 *
 * The constructors are explicit and SHOULD be.
 * This will make future people think about if they're handling a perceived
 * or raw brightness value and treat it acordingly. This will reduce
 * bugs and put the compiler on our side.
 *
 * Perceived and Actual brightness values work on the same
 * [0, maxBrightness] int scale. The maximum brightness value is
 * constant between the two types, so any time a maxBrightness
 * is stored, it can just be an int. The difference in these
 * values is their curve.
 *
 * Conversion works on the principle that:
 *
 *     Perceived Brightness = sqrt(Actual Brightness)
 *
 * To convert, a value is converted to a float [0, 1],
 * squared or rooted, then scaled back up to an int [0, maxBrightness].
 * To convert a value, the maximum brightness value for that device
 * is needed.
 */
struct ActualBrightness;
struct Q_DECL_EXPORT PerceivedBrightness
{
    PerceivedBrightness() = default;
    PerceivedBrightness(const ActualBrightness &a, int maxBrightness);
    explicit PerceivedBrightness(int i);
    explicit operator int() const;

    operator QString() const;

    bool operator==(const PerceivedBrightness &p) const;
    bool operator!=(const PerceivedBrightness &p) const;
    /**
     * Generally, the only brightness values that should
     * be getting compared like this are Perceptual ones.
     */
    bool operator>(const PerceivedBrightness &p) const;
    bool operator<(const PerceivedBrightness &p) const;
    bool operator<=(const PerceivedBrightness &p) const;
    bool operator>=(const PerceivedBrightness &p) const;

private:
    int m_value = -1;
};
Q_DECL_EXPORT PerceivedBrightness operator "" _pb(unsigned long long val);

struct Q_DECL_EXPORT ActualBrightness
{
    ActualBrightness() = default;
    explicit ActualBrightness(int actual);
    ActualBrightness(const PerceivedBrightness &a, int maxBrightness);

    explicit operator int() const;
    operator QString() const;
    bool operator==(const ActualBrightness &a) const;
    bool operator!=(const ActualBrightness &a) const;
private:
    int m_value = -1;
};

Q_DECLARE_METATYPE(PerceivedBrightness);
Q_DECLARE_METATYPE(ActualBrightness);
