/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutildisplay.h"

#include <powerdevil_debug.h>

#define BRIGHTNESS_VCP_FEATURE_CODE 0x10

#ifdef WITH_DDCUTIL
DDCutilDisplay::DDCutilDisplay(DDCA_Display_Ref displayRef)
    : m_displayRef(displayRef)
    , m_brightness(-1)
    , m_maxBrightness(-1)
    , m_supportsBrightness(false)
{
    Q_ASSERT(m_displayRef != nullptr);

    qCDebug(POWERDEVIL) << "[DDCutilDisplay]: Creating display info and handle from display reference... ";
    DDCA_Status status = DDCRC_OK;

    //
    // Part 1: display info

    DDCA_Display_Info *displayInfo = nullptr;
    if (status = ddca_get_display_info(m_displayRef, &displayInfo); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_get_display_info" << status;
        return;
    }
    m_label = displayInfo->model_name;
    m_ioPath = displayInfo->path;

    ddca_free_display_info(displayInfo);

    //
    // Part 2: temporarily opened display handle
    //
    // We don't want to keep it open permanently, because doing so will block other programs
    // backed by libddcutil (like the ddcutil CLI itself) from functioning.

    DDCA_Display_Handle displayHandle = nullptr;
    if (status = ddca_open_display2(m_displayRef, true, &displayHandle); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_open_display2" << status;
        return;
    }

    DDCA_Non_Table_Vcp_Value value;
    if (status = ddca_get_non_table_vcp_value(displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, &value); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_get_non_table_vcp_value" << status;
    } else {
        m_brightness = value.sh << 8 | value.sl;
        m_maxBrightness = value.mh << 8 | value.ml;
        m_supportsBrightness = true;
    }

    if (status = ddca_close_display(displayHandle); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_close_display" << status;
    }
}

DDCA_IO_Path DDCutilDisplay::ioPath() const
{
    return m_ioPath;
}
#endif

DDCutilDisplay::~DDCutilDisplay()
{
}

QString DDCutilDisplay::label() const
{
    return m_label;
}

int DDCutilDisplay::brightness()
{
    QReadLocker lock(&m_lock);

    return m_brightness;
}

int DDCutilDisplay::maxBrightness()
{
    QReadLocker lock(&m_lock);

    return m_maxBrightness;
}

void DDCutilDisplay::setBrightness(int value)
{
    if (!m_supportsBrightness) {
        return;
    }

    QWriteLocker lock(&m_lock);

#ifdef WITH_DDCUTIL
    qCDebug(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "setting brightness to" << value << "with temporary display handle";

    DDCA_Display_Handle displayHandle = nullptr;
    DDCA_Status status = DDCRC_OK;

    if (status = ddca_open_display2(m_displayRef, true, &displayHandle); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_open_display2" << status;
    } else {
        uint8_t sh = value >> 8 & 0xff;
        uint8_t sl = value & 0xff;

        if (status = ddca_set_non_table_vcp_value(displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, sh, sl); status != DDCRC_OK) {
            qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_set_non_table_vcp_value" << status;
        } else {
            m_brightness = value;
        }

        if (DDCA_Status closeStatus = ddca_close_display(displayHandle); closeStatus != DDCRC_OK) {
            qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_close_display" << closeStatus;
            status = closeStatus;
        }
    }
#endif
}

bool DDCutilDisplay::supportsBrightness() const
{
    return m_supportsBrightness;
}

#include "moc_ddcutildisplay.cpp"
