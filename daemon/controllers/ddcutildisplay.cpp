/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutildisplay.h"

#include <powerdevil_debug.h>

#include <chrono>
#include <span>

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds s_setBrightnessDelay = 1s;
constexpr std::array<std::chrono::milliseconds, 3> s_backoffRetryIntervals = {1s, 2s, 3s};

#ifdef WITH_DDCUTIL
constexpr DDCA_Vcp_Feature_Code BRIGHTNESS_VCP_FEATURE_CODE = 0x10;

DDCutilDisplay::DDCutilDisplay(DDCA_Display_Ref displayRef, QMutex *openDisplayMutex)
    : m_displayRef(displayRef)
    , m_brightnessWorker(new BrightnessWorker)
    , m_timer(new QTimer(this))
    , m_retryCounter(0)
    , m_openDisplayMutex(openDisplayMutex)
    , m_brightness(-1)
    , m_maxBrightness(-1)
    , m_supportsBrightness(false)
{
    Q_ASSERT(m_displayRef != nullptr);

    qCDebug(POWERDEVIL) << "[DDCutilDisplay]: Creating display info and handle from display reference...";
    DDCA_Status status = DDCRC_OK;

    //
    // Part 1: display info

    DDCA_Display_Info *displayInfo = nullptr;
    if (status = ddca_get_display_info(m_displayRef, &displayInfo); status != DDCRC_OK) {
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_get_display_info" << status;
        return;
    }
    m_label = QString::fromLocal8Bit(displayInfo->model_name);
    m_ioPath = displayInfo->path;
    m_id = DDCutilDisplay::generatePathId(displayInfo->path);
    // the EDID is always guaranteed to be at least 128 bytes long
    static_assert(sizeof(DDCA_Display_Info::edid_bytes) == 128);
    std::ranges::copy(std::span(displayInfo->edid_bytes, 128), std::back_inserter(m_edidData));

    ddca_free_display_info(displayInfo);

    // Remaining parts in init(), which can be retried if supportsBrightness() is still false
    init();
}

void DDCutilDisplay::init()
{
    // Part 2: temporarily opened display handle
    //
    // We don't want to keep it open permanently, because doing so will block other programs
    // backed by libddcutil (like the ddcutil CLI itself) from functioning.

    {
        // We can't conflict with this object's own brightness setter, but we could conflict with
        // the brightness setter of a different, previously created object running simultaneously.
        QMutexLocker locker(m_openDisplayMutex);

        DDCA_Status status = DDCRC_OK;
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
        if (status != DDCRC_OK || !m_supportsBrightness) {
            return;
        }
    }

    //
    // Part 3: timer & worker setup

    m_timer->setSingleShot(true);
    disconnect(m_timer, &QTimer::timeout, nullptr, nullptr);
    connect(m_timer, &QTimer::timeout, this, &DDCutilDisplay::onSetBrightnessTimeout);

    m_brightnessWorker->moveToThread(&m_brightnessWorkerThread);
    connect(&m_brightnessWorkerThread, &QThread::finished, m_brightnessWorker, &QObject::deleteLater);
    connect(this, &DDCutilDisplay::ddcBrightnessChangeRequested, m_brightnessWorker, &BrightnessWorker::ddcSetBrightness);
    connect(m_brightnessWorker, &BrightnessWorker::ddcBrightnessChangeApplied, this, &DDCutilDisplay::ddcBrightnessChangeFinished);
    m_brightnessWorkerThread.start();

    Q_EMIT supportsBrightnessChanged(true);
}

DDCA_IO_Path DDCutilDisplay::ioPath() const
{
    return m_ioPath;
}

QString DDCutilDisplay::generatePathId(const DDCA_IO_Path &displayPath)
{
    switch (displayPath.io_mode) {
    case DDCA_IO_I2C:
        return QStringLiteral("i2c:%1").arg(displayPath.path.i2c_busno);
    case DDCA_IO_USB:
        return QStringLiteral("usb:%1").arg(displayPath.path.hiddev_devno);
    }
    return QString();
}
#endif

DDCutilDisplay::~DDCutilDisplay()
{
#ifdef WITH_DDCUTIL
    m_brightnessWorkerThread.quit();
    m_brightnessWorkerThread.wait();
#endif
}

QString DDCutilDisplay::id() const
{
    return m_id;
}

QString DDCutilDisplay::label() const
{
    return m_label;
}

int DDCutilDisplay::knownSafeMinBrightness() const
{
    // External monitors are not known to turn off completely when their brightness goes to 0.
    return 0;
}

int DDCutilDisplay::maxBrightness() const
{
    return m_maxBrightness;
}

int DDCutilDisplay::brightness() const
{
    return m_brightness;
}

void DDCutilDisplay::scheduleRetryInit()
{
    disconnect(m_timer, &QTimer::timeout, nullptr, nullptr);
    connect(m_timer, &QTimer::timeout, this, &DDCutilDisplay::onInitRetryTimeout);

    m_retryCounter = 0;
    m_timer->setSingleShot(true);
    m_timer->start(m_supportsBrightness ? 0ms : s_backoffRetryIntervals[m_retryCounter]);

    qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "retrying to initialize DDC/CI brightness in" << m_timer->interval()
                          << "milliseconds - attempt no." << (m_retryCounter + 1);
}

void DDCutilDisplay::onInitRetryTimeout()
{
#ifdef WITH_DDCUTIL
    if (!m_supportsBrightness) {
        init();
    }
    if (!m_supportsBrightness) {
        if (++m_retryCounter; m_retryCounter < s_backoffRetryIntervals.size()) {
            m_timer->start(s_backoffRetryIntervals[m_retryCounter]);

            qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "retrying to initialize DDC/CI brightness in" << m_timer->interval()
                                  << "milliseconds - attempt no." << (m_retryCounter + 1);
            return;
        }
    }
#endif
    qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << (m_supportsBrightness ? "succeeded" : "failed") << "to initialize DDC/CI brightness";
    Q_EMIT retryInitFinished(m_supportsBrightness);
}

void DDCutilDisplay::setBrightness(int value, bool allowAnimations)
{
#ifdef WITH_DDCUTIL
    if (m_supportsBrightness) {
        m_retryCounter = 0;
        m_timer->start(s_setBrightnessDelay);
        m_brightness = value;
    }
#endif
}

void DDCutilDisplay::onSetBrightnessTimeout()
{
    Q_EMIT ddcBrightnessChangeRequested(m_brightness, this);
}

void DDCutilDisplay::ddcBrightnessChangeFinished(bool success)
{
    if (!success) {
        if (m_retryCounter < s_backoffRetryIntervals.size()) {
            m_timer->start(s_backoffRetryIntervals[m_retryCounter]);
            ++m_retryCounter;
            qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "retrying to set DDC/CI brightness in" << m_timer->interval()
                                  << "milliseconds - attempt no." << m_retryCounter;
            return;
        }
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "failed to set DDC/CI brightness";
        m_supportsBrightness = false;
        Q_EMIT supportsBrightnessChanged(false);
    } else if (m_retryCounter > 0) { // only yell if we also logged the "retrying" message
        qCWarning(POWERDEVIL) << "[DDCutilDisplay]:" << m_label << "succeeded to set DDC/CI brightness";
    }
}

void BrightnessWorker::ddcSetBrightness(int value, DDCutilDisplay *display)
{
#ifdef WITH_DDCUTIL
    qCDebug(POWERDEVIL) << "[DDCutilDisplay]:" << display->m_label << "setting brightness to" << value << "with temporary display handle";

    DDCA_Display_Handle displayHandle = nullptr;
    DDCA_Status status = DDCRC_OK;

    {
        QMutexLocker locker(display->m_openDisplayMutex);

        if (status = ddca_open_display2(display->m_displayRef, true, &displayHandle); status != DDCRC_OK) {
            qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_open_display2" << status;
        } else {
            uint8_t sh = value >> 8 & 0xff;
            uint8_t sl = value & 0xff;

            if (status = ddca_set_non_table_vcp_value(displayHandle, BRIGHTNESS_VCP_FEATURE_CODE, sh, sl); status != DDCRC_OK) {
                qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_set_non_table_vcp_value" << status;
            }

            if (DDCA_Status closeStatus = ddca_close_display(displayHandle); closeStatus != DDCRC_OK) {
                qCWarning(POWERDEVIL) << "[DDCutilDisplay]: ddca_close_display" << closeStatus;
                status = closeStatus;
            }
        }
    }

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

    // Allow some delay before starting to work with the monitor
    // because the monitor may not yet be ready to work through DDC/CI after waking up
    m_retryCounter = 0;
    m_timer->start(1000ms);
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

std::optional<QByteArray> DDCutilDisplay::edidData() const
{
#ifdef WITH_DDCUTIL
    return m_edidData;
#else
    return std::nullopt;
#endif
}

#include "moc_ddcutildisplay.cpp"
