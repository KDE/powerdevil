/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutildisplay.h"

#include <chrono>

using namespace std::chrono_literals;

#define BRIGHTNESS_VCP_FEATURE_CODE 0x10

#ifdef WITH_DDCUTIL
DDCutilDisplay::DDCutilDisplay(DDCA_Display_Info displayInfo, DDCA_Display_Handle displayHandle)
    : m_displayHandle(displayHandle)
    , m_label(displayInfo.model_name)
    , m_brightnessWorker(new BrightnessWorker)
    , m_brightness(-1)
    , m_maxBrightness(-1)
    , m_supportsBrightness(false)
{
    Q_ASSERT(displayHandle);

    DDCA_Non_Table_Vcp_Value value;

    if (ddca_get_non_table_vcp_value(m_displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, &value) == DDCRC_OK) {
        m_brightness = value.sh << 8 | value.sl;
        m_maxBrightness = value.mh << 8 | value.ml;
        m_supportsBrightness = true;
    }
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(std::chrono::milliseconds(1000));
    connect(m_timer, &QTimer::timeout, this, &DDCutilDisplay::onTimeout);
    m_brightnessWorker->moveToThread(&m_brightnessWorkerThread);
    connect(&m_brightnessWorkerThread, &QThread::finished, m_brightnessWorker, &QObject::deleteLater);
    connect(this, &DDCutilDisplay::ddcBrightnessChangeRequested, m_brightnessWorker, &BrightnessWorker::ddcSetBrightness);
    connect(m_brightnessWorker, &BrightnessWorker::ddcBrightnessChangeApplied, this, &DDCutilDisplay::ddcBrightnessChangeFinished);
    m_brightnessWorkerThread.start();
}
#endif

DDCutilDisplay::~DDCutilDisplay()
{
#ifdef WITH_DDCUTIL
    m_brightnessWorkerThread.quit();
    m_brightnessWorkerThread.wait();
    ddca_close_display(m_displayHandle);
#endif
}

QString DDCutilDisplay::label() const
{
    return m_label;
}

int DDCutilDisplay::brightness()
{
    return m_brightness;
}

int DDCutilDisplay::maxBrightness()
{
    return m_maxBrightness;
}

void DDCutilDisplay::setBrightness(int value)
{
#ifdef WITH_DDCUTIL
    m_timer->start();
    m_brightness = value;
    Q_EMIT brightnessChanged(value, m_maxBrightness);
#endif
}

void DDCutilDisplay::ddcBrightnessChangeFinished(bool isSuccessful)
{
    if (!isSuccessful) {
        m_supportsBrightness = false;
        Q_EMIT supportsBrightnessChanged(false);
    }
}

void BrightnessWorker::ddcSetBrightness(int value, DDCutilDisplay *display)
{
#ifdef WITH_DDCUTIL
    uint8_t sh = value >> 8 & 0xff;
    uint8_t sl = value & 0xff;

    auto status = ddca_set_non_table_vcp_value(display->m_displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, sh, sl);
    Q_EMIT ddcBrightnessChangeApplied(status == DDCRC_OK);
#endif
}

bool DDCutilDisplay::supportsBrightness() const
{
    return m_supportsBrightness;
}

void DDCutilDisplay::resumeWorker()
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    connect(this, &DDCutilDisplay::ddcBrightnessChangeRequested, m_brightnessWorker, &BrightnessWorker::ddcSetBrightness);
    Q_EMIT ddcBrightnessChangeRequested(m_brightness, this);
#endif
#endif
}

void DDCutilDisplay::pauseWorker()
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    disconnect(this, &DDCutilDisplay::ddcBrightnessChangeRequested, m_brightnessWorker, &BrightnessWorker::ddcSetBrightness);
#endif
#endif
}

void DDCutilDisplay::onTimeout()
{
#ifdef WITH_DDCUTIL
    Q_EMIT ddcBrightnessChangeRequested(m_brightness, this);
#endif
}

#include "moc_ddcutildisplay.cpp"
