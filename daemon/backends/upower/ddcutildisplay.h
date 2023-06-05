/*  This file is part of the KDE project
 *    Copyright (C) 2023 Quang Ngô <ngoquang2708@gmail.com>
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

#pragma once

#include <QObject>
#include <QReadWriteLock>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>
#endif

class DDCutilDisplay : public QObject
{
    Q_OBJECT
public:
#ifdef WITH_DDCUTIL
    DDCutilDisplay(DDCA_Display_Info, DDCA_Display_Handle);
#endif
    ~DDCutilDisplay();

    QString label() const;
    int brightness();
    int maxBrightness();
    void setBrightness(int value);

private:
#ifdef WITH_DDCUTIL
    DDCA_Display_Info m_displayInfo;
    DDCA_Display_Handle m_displayHandle;
#endif
    QReadWriteLock m_lock;
    int m_brightness;
    int m_maxBrightness;
};
