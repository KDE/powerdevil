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
}

DDCutilBrightness::~DDCutilBrightness()
{
    for (auto &display : m_displays) {
        delete display;
    }
    m_displays.clear();
}

void DDCutilBrightness::detect()
{
#ifdef WITH_DDCUTIL
    qCDebug(POWERDEVIL) << "Check for monitors using ddca_get_displays()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List *displays = nullptr;
    ddca_get_display_info_list2(true, &displays);
    qCInfo(POWERDEVIL)  << "[DDCutilBrightness]" << displays->ct << "display(s) were detected";

    for (auto &displayInfo : std::span(displays->info, displays->ct)) {
        DDCA_Display_Handle displayHandle = nullptr;
        DDCA_Status rc;

        qCDebug(POWERDEVIL) << "Opening the display reference, creating a display handle...";
        if ((rc = ddca_open_display2(displayInfo.dref, true, &displayHandle))) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_open_display2" << rc;
            continue;
        }

        QList<uint16_t> supportedFeatures;
        for (int usedVcpIndex = 0; usedVcpIndex < m_usedVcp.count(); ++usedVcpIndex) {
            DDCA_Non_Table_Vcp_Value value;
            if ((rc = ddca_get_non_table_vcp_value(displayHandle, m_usedVcp.value(usedVcpIndex), &value))) {
                qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor does not seem to support" << m_usedVcp[usedVcpIndex];
            } else {
                qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor supports" << m_usedVcp[usedVcpIndex];
                supportedFeatures.append(m_usedVcp.value(usedVcpIndex));
            }
        }

        if (supportedFeatures.contains(0x10)) {
            qCDebug(POWERDEVIL) << "Display supports Brightness, adding handle to list";
            QString displayId = generateDisplayId(displayInfo);
            qCDebug(POWERDEVIL) << "Create a Display Identifier:" << displayId << "for display:" << displayInfo.model_name;

            if (displayId.isEmpty()) {
                qCWarning(POWERDEVIL) << "Cannot generate ID for display with model name:" << displayInfo.model_name;
                continue;
            }

            m_displays[displayId] = new DDCutilDisplay(displayInfo, displayHandle);
            m_displayIds += displayId;
            continue;
        }
        ddca_close_display(displayHandle);
    }
    ddca_free_display_info_list(displays);
#else
    qCInfo(POWERDEVIL) << "[DDCutilBrightness] compiled without DDC/CI support";
    return;
#endif
}

bool DDCutilBrightness::isSupported() const
{
#ifdef WITH_DDCUTIL
    return !m_displayIds.isEmpty();
#else
    return false;
#endif
}

int DDCutilBrightness::brightness()
{
    auto const &displayId = m_displayIds.constFirst();
    return m_displays[displayId]->brightness();
}

int DDCutilBrightness::brightnessMax()
{
    auto const &displayId = m_displayIds.constFirst();
    return m_displays[displayId]->maxBrightness();
}

void DDCutilBrightness::setBrightness(int value)
{
    auto const &displayId = m_displayIds.constFirst();
    qCDebug(POWERDEVIL) << "setBrightness: displayId:" << displayId << "brightness:" << value;
    m_displays[displayId]->setBrightness(value);
}

#ifdef WITH_DDCUTIL
QString DDCutilBrightness::generateDisplayId(const DDCA_Display_Info &displayInfo) const
{
    switch (displayInfo.path.io_mode) {
    case DDCA_IO_I2C:
        return QString("i2c:%1").arg(displayInfo.path.path.i2c_busno);
    case DDCA_IO_USB:
        return QString("usb:%1").arg(displayInfo.path.path.hiddev_devno);
    case DDCA_IO_ADL:
        return QString("adl:%1:%2").arg(displayInfo.path.path.adlno.iAdapterIndex, displayInfo.path.path.adlno.iDisplayIndex);;
    }
    return QString();
}
#endif

