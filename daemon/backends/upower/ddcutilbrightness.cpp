/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutilbrightness.h"
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
    qCDebug(POWERDEVIL) << "Check for monitors using ddca_get_displays()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List *displays = nullptr;
    ddca_get_display_info_list2(true, &displays);
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
