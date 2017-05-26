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
    //TODO: implement
    SysfsBrightness() {};
    //TODO: implement
    bool isSupported() const {return false;};
    //TODO: implement
    long brightness() const {return 1;};
    //TODO: implement
    void brightnessMax(long value) {};
};

#endif /* end of include guard: SYSFSBRIGHTNESS_H */
