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

#include <powerdevil_debug.h>
#include "ddcutilbrightness.h"
#include <chrono>

using namespace std::chrono_literals;

DDCutilBrightness::DDCutilBrightness()
    : m_usedVcp({0x10})
{
    m_setBrightnessEventFilter.setInterval(100ms);
    m_setBrightnessEventFilter.setSingleShot(true);
    connect(&m_setBrightnessEventFilter, &QTimer::timeout, this, &DDCutilBrightness::setBrightnessAfterFilter);
}

DDCutilBrightness::~DDCutilBrightness()
{
#ifdef WITH_DDCUTIL
    for (const DDCA_Display_Handle displayHandle : std::as_const(m_displayHandleList)) {
        ddca_close_display(displayHandle);
    }
    m_displayHandleList.clear();
#endif
}

void DDCutilBrightness::detect()
{
#ifdef WITH_DDCUTIL
    DDCA_Status rc;

    qCDebug(POWERDEVIL) << "Check for monitors using ddca_get_displays()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List * dlist = nullptr;
    ddca_get_display_info_list2(true, &dlist);
    qCInfo(POWERDEVIL)  << "[DDCutilBrightness] " << dlist->ct << "display(s) were detected";

    for (const DDCA_Display_Handle displayHandle : std::as_const(m_displayHandleList)) {
        ddca_close_display(displayHandle);
    }
    m_displayHandleList.clear();
    for (int iDisp = 0; iDisp < dlist->ct; ++iDisp) {
        DDCA_Display_Handle dh = nullptr;  // initialize to avoid clang analyzer warning

        qCDebug(POWERDEVIL) << "Create a Display Identifier for display"<<iDisp <<
        " : "<< dlist->info[iDisp].model_name;

        m_displayInfoList.append(dlist->info[iDisp]);

        qCDebug(POWERDEVIL) << "Opening the display reference, creating a display handle...";
        rc = ddca_open_display2(dlist->info[iDisp].dref, true, &dh);
        if (rc != 0) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_open_display2" << rc;
            continue;
        }

        DDCA_Feature_List vcpList;
        ddca_get_feature_list_by_dref(DDCA_SUBSET_COLOR, dh, false, &vcpList);
        QVector<uint16_t> tmpVcpList;
        for (int iVcp = 0; iVcp < m_usedVcp.count(); ++iVcp) {
            DDCA_Non_Table_Vcp_Value returnValue;
            rc = ddca_get_non_table_vcp_value(dh,m_usedVcp.value(iVcp), &returnValue);

            if (rc < 0) {
                qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor does not seem to support " << m_usedVcp[iVcp];
            } else {
                qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor supports " << m_usedVcp[iVcp];
                tmpVcpList.append(m_usedVcp.value(iVcp));
            }
        }
        //we only store displays that actually support the features we want.
        if (tmpVcpList.contains(0x10)) {
            qCDebug(POWERDEVIL) << "Display supports Brightness, adding handle to list";
            m_displayHandleList.append(dh);
            m_supportedVcp_perDisp.value(iDisp).append(tmpVcpList);
        }
    }
#else
    qCInfo(POWERDEVIL)  << "[DDCutilBrightness] compiled without DDC/CI support";
    return;
#endif
}

bool DDCutilBrightness::isSupported() const
{
#ifdef WITH_DDCUTIL
    return !m_displayHandleList.isEmpty();
#else
    return false;
#endif
}

long DDCutilBrightness::brightness()
{
#ifdef WITH_DDCUTIL
    //we check whether the timer is running, this means we received new values but did not send them yet to the monitor
    //not checking that results in the brightness slider jump to the previous value when changing.
    if (m_setBrightnessEventFilter.isActive()) {
        m_lastBrightnessKnown = m_tmpCurrentBrightness;
    } else {  //FIXME: gets value only for first display
        DDCA_Status rc;
        DDCA_Non_Table_Vcp_Value returnValue;

        rc = ddca_get_non_table_vcp_value(m_displayHandleList.at(0),
                                0x10, &returnValue);
        qCDebug(POWERDEVIL) << "[DDCutilBrightness::brightness]: ddca_get_vcp_value returned" << rc;

        //check rc to prevent crash on wake from idle and the monitor has gone to powersave mode
        if (rc == 0) {
            m_lastBrightnessKnown = (long)(returnValue.sh <<8|returnValue.sl);
        }
    }
    return m_lastBrightnessKnown;
#else
    return 0;
#endif
}

long DDCutilBrightness::brightnessMax()
{
#ifdef WITH_DDCUTIL
    DDCA_Status rc;
    DDCA_Non_Table_Vcp_Value returnValue;

    rc = ddca_get_non_table_vcp_value(m_displayHandleList.at(0)
                                    , 0x10, &returnValue);
    qCDebug(POWERDEVIL) << "[DDCutilBrightness::brightnessMax]: ddca_get_vcp_value returned" << rc;

    //check rc to prevent crash on wake from idle and the monitor has gone to powersave mode
    if (rc == 0) {
        m_lastMaxBrightnessKnown = (long)(returnValue.mh <<8|returnValue.ml);
    }

    return m_lastMaxBrightnessKnown;
#else
    return 100.0;
#endif
}

void DDCutilBrightness::setBrightness(long value)
{
    m_tmpCurrentBrightness = value;
    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: saving brightness value: " << value;
    m_setBrightnessEventFilter.start();
}

void DDCutilBrightness::setBrightnessAfterFilter()
{
#ifdef WITH_DDCUTIL
    bool with_errors = false;
    DDCA_Status rc;
    for (int iDisp = 0; iDisp < m_displayHandleList.count(); ++iDisp) {          //FIXME: we set the same brightness to all monitors, plugin architecture needed
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: setting brightness " << m_tmpCurrentBrightness;
        uint8_t newsh = (uint16_t)m_tmpCurrentBrightness >> 8;
        uint8_t newsl = (uint16_t)m_tmpCurrentBrightness & 0x00ff;
        rc = ddca_set_non_table_vcp_value(m_displayHandleList.at(iDisp), 0x10, newsh, newsl);
        if (rc < 0) {
            with_errors = true;
        }
    }
    if (with_errors) {
        qCWarning(POWERDEVIL) << "[DDCutilBrightness::setBrightness] failed, trying to detect()";
        detect();
    } else {
        m_lastBrightnessKnown = m_tmpCurrentBrightness;
    }
#else
    return;
#endif
}
