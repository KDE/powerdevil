/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
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
    bool supportsBrightness() const;

private:
#ifdef WITH_DDCUTIL
    DDCA_Display_Info m_displayInfo;
    DDCA_Display_Handle m_displayHandle;
#endif
    QReadWriteLock m_lock;
    int m_brightness;
    int m_maxBrightness;
    bool m_supportsBrightness;
};
