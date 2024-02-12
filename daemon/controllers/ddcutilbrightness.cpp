/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "ddcutilbrightness.h"

#include <powerdevil_debug.h>

#include <span>
#include <unordered_map>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>

#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
void ddcaCallback(DDCA_Display_Status_Event event);
#endif

class DDCutilPrivateSingleton : public QObject
{
    Q_OBJECT

#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    friend void ddcaCallback(DDCA_Display_Status_Event event);
#endif
    friend class DDCutilBrightness;

public:
    static DDCutilPrivateSingleton &instance();

private:
    explicit DDCutilPrivateSingleton();
    ~DDCutilPrivateSingleton();

    QString generateDisplayId(const DDCA_IO_Path &displayPath) const;
    void detect();
    QStringList displayIds() const;
    std::unordered_map<QString, std::unique_ptr<DDCutilDisplay>> &displays();
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    void displayStatusChanged(DDCA_Display_Status_Event &event);
#endif
Q_SIGNALS:
    void displayAdded();
    void displayRemoved(QString path);
    void dpmsStateChanged(QString path, bool isSleeping);

private Q_SLOTS:
    void redetect();
    void removeDisplay(const QString &path);
    void setDpmsState(const QString &path, bool isSleeping);

private:
    QStringList m_displayIds;
    std::unordered_map<QString, std::unique_ptr<DDCutilDisplay>> m_displays;
};

#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
void ddcaCallback(DDCA_Display_Status_Event event)
{
    DDCutilPrivateSingleton::instance().displayStatusChanged(event);
}
#endif

DDCutilPrivateSingleton::DDCutilPrivateSingleton()
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 0, 0)
    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Initializing ddcutil API (create ddcutil configuration file for tracing & more)...";
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    DDCA_Status init_status = ddca_init2(nullptr, DDCA_SYSLOG_NOTICE, DDCA_INIT_OPTIONS_CLIENT_OPENED_SYSLOG, nullptr);
#else
    DDCA_Status init_status = ddca_init(nullptr, DDCA_SYSLOG_NOTICE, DDCA_INIT_OPTIONS_CLIENT_OPENED_SYSLOG);
#endif
    if (init_status < 0) {
        qCWarning(POWERDEVIL) << "[DDCutilBrightness]: Could not initialize ddcutil API. Not using DDC for monitor brightness.";
        return;
    }
#endif
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (ddca_register_display_status_callback(ddcaCallback)) {
        qCWarning(POWERDEVIL) << "[DDCutilBrightness]: Failed to initialize callback";
        return;
    }

    connect(this, &DDCutilPrivateSingleton::displayAdded, this, &DDCutilPrivateSingleton::redetect);
    connect(this, &DDCutilPrivateSingleton::displayRemoved, this, &DDCutilPrivateSingleton::removeDisplay);
    connect(this, &DDCutilPrivateSingleton::dpmsStateChanged, this, &DDCutilPrivateSingleton::setDpmsState);

    ddca_start_watch_displays(DDCA_Display_Event_Class(DDCA_EVENT_CLASS_ALL));
#endif
}

DDCutilPrivateSingleton::~DDCutilPrivateSingleton()
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    ddca_stop_watch_displays(false);
    ddca_unregister_display_status_callback(ddcaCallback);
    disconnect(this, &DDCutilPrivateSingleton::displayAdded, this, &DDCutilPrivateSingleton::redetect);
    disconnect(this, &DDCutilPrivateSingleton::displayRemoved, this, &DDCutilPrivateSingleton::removeDisplay);
    disconnect(this, &DDCutilPrivateSingleton::dpmsStateChanged, this, &DDCutilPrivateSingleton::setDpmsState);
#endif
}

DDCutilPrivateSingleton &DDCutilPrivateSingleton::instance()
{
    static DDCutilPrivateSingleton singleton;
    return singleton;
}

void DDCutilPrivateSingleton::detect()
{
    if (qEnvironmentVariableIntValue("POWERDEVIL_NO_DDCUTIL") > 0) {
        return;
    }

    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Check for monitors using ddca_get_display_info_list2()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List *displays = nullptr;
    ddca_get_display_info_list2(false, &displays);
    qCInfo(POWERDEVIL) << "[DDCutilBrightness]" << displays->ct << "display(s) were detected";

    for (auto &displayInfo : std::span(displays->info, displays->ct)) {
        DDCA_Display_Handle displayHandle = nullptr;
        DDCA_Status rc;

        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Opening the display reference, creating a display handle...";
        if ((rc = ddca_open_display2(displayInfo.dref, true, &displayHandle))) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_open_display2" << rc;
            continue;
        }

        auto display = std::make_unique<DDCutilDisplay>(displayInfo, displayHandle);

        if (!display->supportsBrightness()) {
            qCDebug(POWERDEVIL) << "[DDCutilBrightness]: This monitor does not seem to support brightness control";
            continue;
        }

        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Display supports Brightness, adding handle to list";
        QString displayId = generateDisplayId(displayInfo.path);
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Create a Display Identifier:" << displayId << "for display:" << displayInfo.model_name;

        if (displayId.isEmpty()) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: Cannot generate ID for display with model name:" << displayInfo.model_name;
            continue;
        }

        m_displays[displayId] = std::move(display);
        m_displayIds += displayId;
    }
    ddca_free_display_info_list(displays);
}

QStringList DDCutilPrivateSingleton::displayIds() const
{
    return m_displayIds;
}

std::unordered_map<QString, std::unique_ptr<DDCutilDisplay>> &DDCutilPrivateSingleton::displays()
{
    return m_displays;
}

QString DDCutilPrivateSingleton::generateDisplayId(const DDCA_IO_Path &displayPath) const
{
    switch (displayPath.io_mode) {
    case DDCA_IO_I2C:
        return QString("i2c:%1").arg(displayPath.path.i2c_busno);
    case DDCA_IO_USB:
        return QString("usb:%1").arg(displayPath.path.hiddev_devno);
    }
    return QString();
}

void DDCutilPrivateSingleton::redetect()
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Screen configuration changed. Redetecting displays";

    m_displays.clear();
    m_displayIds.clear();

    if (ddca_redetect_displays()) {
        qCCritical(POWERDEVIL) << "[DDCutilBrightness]: Redetection failed";
        return;
    }

    detect();
#endif
}

#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
void DDCutilPrivateSingleton::displayStatusChanged(DDCA_Display_Status_Event &event)
{
    if (event.event_type == DDCA_EVENT_DISPLAY_CONNECTED) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: DDCA_EVENT_DISPLAY_CONNECTED signal arived";
        Q_EMIT displayAdded();
    } else if (event.event_type == DDCA_EVENT_DISPLAY_DISCONNECTED) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: DDCA_EVENT_DISPLAY_DISCONNECTED signal arived";
        Q_EMIT displayRemoved(generateDisplayId(event.io_path));
    } else if (event.event_type == DDCA_EVENT_DPMS_ASLEEP) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: DDCA_EVENT_DPMS_ASLEEP signal arived";
        Q_EMIT dpmsStateChanged(generateDisplayId(event.io_path), true);
    } else if (event.event_type == DDCA_EVENT_DPMS_AWAKE) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: DDCA_EVENT_DPMS_AWAKE signal arived";
        Q_EMIT dpmsStateChanged(generateDisplayId(event.io_path), false);
    }
}
#endif

void DDCutilPrivateSingleton::removeDisplay(const QString &path)
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (m_displayIds.contains(path)) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: closing display" << path;
        m_displayIds.remove(m_displayIds.indexOf(path));
        m_displays.erase(path);
    } else {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Remove display failded";
    }
#endif
}

void DDCutilPrivateSingleton::setDpmsState(const QString &path, bool isSleeping)
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (m_displayIds.contains(path)) {
        if (!isSleeping) {
            m_displays.at(path)->wakeWorker();
            qCDebug(POWERDEVIL) << "[DDCutilBrightness]: wakeup displays";
        }

        m_displays.at(path)->setIsSleeping(isSleeping);
    }
#endif
}

#endif

DDCutilBrightness::DDCutilBrightness(QObject *parent)
    : QObject(parent)
{
}

DDCutilBrightness::~DDCutilBrightness()
{
}

bool DDCutilBrightness::isSupported() const
{
#ifdef WITH_DDCUTIL
    return !DDCutilPrivateSingleton::instance().displayIds().isEmpty();
#else
    return false;
#endif
}

int DDCutilBrightness::brightness(const QString &displayId)
{
#ifdef WITH_DDCUTIL
    if (!DDCutilPrivateSingleton::instance().displayIds().contains(displayId)) {
        return -1;
    }
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (!DDCutilPrivateSingleton::instance().displays()[displayId]->supportsBrightness()) {
        DDCutilPrivateSingleton::instance().removeDisplay(displayId);
        return -1;
    }
#endif
    return DDCutilPrivateSingleton::instance().displays()[displayId]->brightness();
#else
    return -1;
#endif
}

int DDCutilBrightness::maxBrightness(const QString &displayId)
{
#ifdef WITH_DDCUTIL
    if (!DDCutilPrivateSingleton::instance().displayIds().contains(displayId)) {
        return -1;
    }
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (!DDCutilPrivateSingleton::instance().displays()[displayId]->supportsBrightness()) {
        DDCutilPrivateSingleton::instance().removeDisplay(displayId);
        return -1;
    }
#endif
    return DDCutilPrivateSingleton::instance().displays()[displayId]->maxBrightness();
#else
    return -1;
#endif
}

void DDCutilBrightness::setBrightness(const QString &displayId, int value)
{
#ifdef WITH_DDCUTIL
    if (!DDCutilPrivateSingleton::instance().displayIds().contains(displayId)) {
        return;
    }
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (!DDCutilPrivateSingleton::instance().displays()[displayId]->supportsBrightness()) {
        DDCutilPrivateSingleton::instance().removeDisplay(displayId);
        return;
    }
#endif
    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: setBrightness: displayId:" << displayId << "brightness:" << value;
    DDCutilPrivateSingleton::instance().displays()[displayId]->setBrightness(value);
#endif
}

QStringList DDCutilBrightness::displayIds() const
{
#ifdef WITH_DDCUTIL
    return DDCutilPrivateSingleton::instance().displayIds();
#else
    return QStringList();
#endif
}

void DDCutilBrightness::detect()
{
#ifdef WITH_DDCUTIL
    DDCutilPrivateSingleton::instance().detect();
#else
    qCInfo(POWERDEVIL) << "[DDCutilBrightness] compiled without DDC/CI support";
#endif
}

#include "ddcutilbrightness.moc"
#include "moc_ddcutilbrightness.cpp"
