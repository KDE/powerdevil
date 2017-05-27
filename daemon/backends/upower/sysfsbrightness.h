/*  This file is part of the KDE project
 *    Copyright (C) 2017 Bhushan Shah <bshah@kde.org>
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License version 2 as published by the Free Software Foundation.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 *
 */

#ifndef SYSFSBRIGHTNESS_H
#define SYSFSBRIGHTNESS_H

#include <QObject>

class SysfsBrightness: public QObject
{
    Q_OBJECT
public:
    SysfsBrightness();
    void detect();
    bool isSupported() { return m_isSupported; };
    int brightness() const {
        return m_brightness;
    };
    int brightnessMax() {
        return m_brightnessMax;
    };
    void setBrightness(int value);

private:
    int m_brightness = 0;
    int m_brightnessMax = 0;
    QString m_syspath;
    bool m_isLedBrightnessControl;
    bool m_isSupported = false;
};

#endif /* end of include guard: SYSFSBRIGHTNESS_H */
