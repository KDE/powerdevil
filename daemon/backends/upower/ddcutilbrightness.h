/*  This file is part of the KDE project
 *    Copyright (C) 2017 Dorian Vogel <dorianvogel@gmail.com>
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

#include <QHash>
#include <QList>
#include <QObject>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>
#endif

#include "ddcutildisplay.h"

class DDCutilBrightness: public QObject
{
    Q_OBJECT
public:
    DDCutilBrightness();
    ~DDCutilBrightness();

    void detect();
    QStringList displayIds() const;
    bool isSupported() const;
    int brightness(const QString &displayId);
    int brightnessMax(const QString &displayId);
    void setBrightness(const QString &displayId, int value);

private:
#ifdef WITH_DDCUTIL
    QString generateDisplayId(const DDCA_Display_Info &displayInfo) const;
#endif

private:
    QStringList m_displayIds;
    QHash<QString, DDCutilDisplay *> m_displays;
};
