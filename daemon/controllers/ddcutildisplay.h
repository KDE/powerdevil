/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2023 Quang Ngô <ngoquang2708@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include <QObject>
#include <QReadWriteLock>
#include <QThread>
#include <QWaitCondition>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>
#include <ddcutil_macros.h> // for DDCUTIL_V{MAJOR,MINOR,MICRO}
#include <ddcutil_status_codes.h>

#define DDCUTIL_VERSION QT_VERSION_CHECK(DDCUTIL_VMAJOR, DDCUTIL_VMINOR, DDCUTIL_VMICRO)
#endif

class BrightnessWorker;

class DDCutilDisplay : public QObject
{
    Q_OBJECT

    friend class BrightnessWorker;

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
    void setIsSleeping(bool isSleeping);
    bool isSleeping() const;
    void wakeWorker();

Q_SIGNALS:
    void ddcBrightnessChangeRequested(int value, DDCutilDisplay *display);

private Q_SLOTS:
    void onDdcBrightnessChangeFinished(int brightness, bool isSuccessful);

private:
#ifdef WITH_DDCUTIL
    DDCA_Display_Handle m_displayHandle;
#endif
    QString m_label;
    BrightnessWorker *m_brightnessWorker;
    QThread m_brightnessWorkerThread;
    QWaitCondition m_sync;
    QReadWriteLock m_lock;
    int m_brightness;
    int m_maxBrightness;
    int m_cachedBrightness;
    bool m_supportsBrightness;
    bool m_isSleeping;
    bool m_isWorking;
};

class BrightnessWorker : public QObject
{
    Q_OBJECT
    friend class DDCutilDisplay;

private Q_SLOTS:
    void ddcSetBrightness(int value, DDCutilDisplay *display);
Q_SIGNALS:
    void ddcBrightnessChangeApplied(int value, bool isSuccessful);
};