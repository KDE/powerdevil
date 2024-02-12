/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutildisplay.h"

#define BRIGHTNESS_VCP_FEATURE_CODE 0x10

#ifdef WITH_DDCUTIL
DDCutilDisplay::DDCutilDisplay(DDCA_Display_Info displayInfo, DDCA_Display_Handle displayHandle)
    : m_displayInfo(displayInfo)
    , m_displayHandle(displayHandle)
    , m_brightnessWorker(new BrightnessWorker)
    , m_brightness(-1)
    , m_maxBrightness(-1)
    , m_supportsBrightness(false)
    , m_isSleeping(false)
    , m_isWorking(false)
{
    Q_ASSERT(displayHandle);

    DDCA_Non_Table_Vcp_Value value;

    if (ddca_get_non_table_vcp_value(m_displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, &value) == DDCRC_OK) {
        m_brightness = value.sh << 8 | value.sl;
        m_maxBrightness = value.mh << 8 | value.ml;
        m_supportsBrightness = true;
    }
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    m_brightnessWorker->moveToThread(&m_brightnessWorkerThread);
    connect(&m_brightnessWorkerThread, &QThread::finished, m_brightnessWorker, &QObject::deleteLater);
    connect(this, &DDCutilDisplay::ddcBrightnessChangeRequested, m_brightnessWorker, &BrightnessWorker::ddcSetBrightness);
    connect(m_brightnessWorker, &BrightnessWorker::ddcBrightnessChangeApplied, this, &DDCutilDisplay::onDdcBrightnessChangeFinished);
    m_brightnessWorkerThread.start();
#endif
}
#endif

DDCutilDisplay::~DDCutilDisplay()
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    m_brightnessWorkerThread.quit();
    m_brightnessWorkerThread.wait();
#endif
    ddca_close_display(m_displayHandle);
#endif
}

QString DDCutilDisplay::label() const
{
#ifdef WITH_DDCUTIL
    return m_displayInfo.model_name;
#else
    return QString();
#endif
}

int DDCutilDisplay::brightness()
{
    QReadLocker lock(&m_lock);
    return m_brightness;
}

int DDCutilDisplay::maxBrightness()
{
    return m_maxBrightness;
}

void DDCutilDisplay::setBrightness(int value)
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    m_cachedBrightness = value;

    if (!m_isWorking) {
        m_isWorking = true;
        Q_EMIT ddcBrightnessChangeRequested(value, this);
    }
#else
    QWriteLocker lock(&m_lock);

    uint8_t sh = value >> 8 & 0xff;
    uint8_t sl = value & 0xff;

    if (ddca_set_non_table_vcp_value(m_displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, sh, sl) == DDCRC_OK) {
        m_brightness = value;
    }
#endif
#endif
}

void DDCutilDisplay::onDdcBrightnessChangeFinished(int brightness, bool isSuccessful)
{
    if (isSuccessful) {
        if (m_cachedBrightness != brightness) {
            Q_EMIT ddcBrightnessChangeRequested(m_cachedBrightness, this);
        } else {
            m_isWorking = false;
        }
    } else {
        m_supportsBrightness = false;
    }
}

void BrightnessWorker::ddcSetBrightness(int value, DDCutilDisplay *display)
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    display->m_lock.lockForWrite();

    uint8_t sh = value >> 8 & 0xff;
    uint8_t sl = value & 0xff;

    if (display->isSleeping()) {
        display->m_sync.wait(&display->m_lock, 15000);
    }

    if (ddca_set_non_table_vcp_value(display->m_displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, sh, sl) == DDCRC_OK) {
        display->m_brightness = value;
        Q_EMIT ddcBrightnessChangeApplied(value, true);
    } else {
        Q_EMIT ddcBrightnessChangeApplied(0, false);
    }

    display->m_lock.unlock();
#endif
#endif
}

bool DDCutilDisplay::supportsBrightness() const
{
    return m_supportsBrightness;
}

void DDCutilDisplay::setIsSleeping(bool isSleeping)
{
    m_isSleeping = isSleeping;
}

bool DDCutilDisplay::isSleeping() const
{
    return m_isSleeping;
}

void DDCutilDisplay::wakeWorker()
{
#ifdef WITH_DDCUTIL
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    m_sync.wakeAll();
#endif
#endif
}

#include "moc_ddcutildisplay.cpp"
