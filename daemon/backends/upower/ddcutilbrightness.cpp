/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutilbrightness.h"

#ifdef WITH_DDCUTIL
#include <ddcutil_macros.h> // for DDCUTIL_V{MAJOR,MINOR,MICRO}
#define DDCUTIL_VERSION QT_VERSION_CHECK(DDCUTIL_VMAJOR, DDCUTIL_VMINOR, DDCUTIL_VMICRO)
#endif

#include <chrono>
#include <powerdevil_debug.h>
#include <span>

using namespace std::chrono_literals;

DDCutilBrightness::DDCutilBrightness()
{
}

DDCutilBrightness::~DDCutilBrightness()
{
}

void DDCutilBrightness::detect()
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 0, 0)
    qCDebug(POWERDEVIL) << "Initializing ddcutil API (create ddcutil configuration file for tracing & more)...";
    static DDCA_Status init_status = -1; // negative indicates error condition
    if (init_status < 0) {
        init_status = ddca_init(nullptr, DDCA_SYSLOG_NOTICE, DDCA_INIT_OPTIONS_CLIENT_OPENED_SYSLOG);
    }
    if (init_status < 0) {
        qCWarning(POWERDEVIL) << "Could not initialize ddcutil API. Not using DDC for monitor brightness.";
        return;
    }
#endif
    qCDebug(POWERDEVIL) << "Check for monitors using ddca_get_display_info_list2()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List *displays = nullptr;
    ddca_get_display_info_list2(false, &displays);
    qCInfo(POWERDEVIL) << "[DDCutilBrightness]" << displays->ct << "display(s) were detected";

    for (auto &displayInfo : std::span(displays->info, displays->ct)) {
        DDCA_Display_Handle displayHandle = nullptr;
        DDCA_Status rc;

        qCDebug(POWERDEVIL) << "Opening the display reference, creating a display handle...";
        if ((rc = ddca_open_display2(displayInfo.dref, true, &displayHandle))) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_open_display2" << rc;
            continue;
        }

        auto display = std::make_unique<DDCutilDisplay>(displayInfo, displayHandle);

        if (!display->supportsBrightness()) {
            qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor does not seem to support brightness control";
            continue;
        }

        qCDebug(POWERDEVIL) << "Display supports Brightness, adding handle to list";
        QString displayId = generateDisplayId(displayInfo);
        qCDebug(POWERDEVIL) << "Create a Display Identifier:" << displayId << "for display:" << displayInfo.model_name;

        if (displayId.isEmpty()) {
            qCWarning(POWERDEVIL) << "Cannot generate ID for display with model name:" << displayInfo.model_name;
            continue;
        }

        m_displays[displayId] = std::move(display);
        m_displayIds += displayId;
    }
    ddca_free_display_info_list(displays);
#else
    qCInfo(POWERDEVIL) << "[DDCutilBrightness] compiled without DDC/CI support";
    return;
#endif
}

QStringList DDCutilBrightness::displayIds() const
{
    return m_displayIds;
}

bool DDCutilBrightness::isSupported() const
{
#ifdef WITH_DDCUTIL
    return !m_displayIds.isEmpty();
#else
    return false;
#endif
}

int DDCutilBrightness::brightness(const QString &displayId)
{
    if (!m_displayIds.contains(displayId)) {
        return -1;
    }

    return m_displays[displayId]->brightness();
}

int DDCutilBrightness::brightnessMax(const QString &displayId)
{
    if (!m_displayIds.contains(displayId)) {
        return -1;
    }

    return m_displays[displayId]->maxBrightness();
}

void DDCutilBrightness::setBrightness(const QString &displayId, int value)
{
    if (!m_displayIds.contains(displayId)) {
        return;
    }

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
    }
    return QString();
}
#endif

#include "moc_ddcutilbrightness.cpp"
