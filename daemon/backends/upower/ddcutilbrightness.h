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

#ifndef DDCUTILBRIGHTNESS_H
#define DDCUTILBRIGHTNESS_H

#include <QObject>
#include <QVector>
#include <QTimer>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>
#endif

class DDCutilBrightness: public QObject
{
    Q_OBJECT
public:
    DDCutilBrightness();
    void detect();
    bool isSupported() const;
    long brightness();
    long brightnessMax();
    void setBrightness(long value);

private Q_SLOTS:
    void setBrightnessAfterFilter();

private:
#ifdef WITH_DDCUTIL
    QVector<DDCA_Display_Handle> m_displayHandleList;
    QVector<DDCA_Display_Info> m_displayInfoList;
#endif //ifdef WITH_DDCUTIL
    //Per display properties
    //destription mapped to vcp values for easy retrieval
    QVector<int> m_usedVcp;
    QVector<QVector<uint16_t>> m_supportedVcp_perDisp;

    long  m_tmpCurrentBrightness;
    QTimer m_setBrightnessEventFilter;
    int m_lastBrightnessKnown;
    int m_lastMaxBrightnessKnown;
};

#endif //DDCUTILBRIGHTNESS_H
