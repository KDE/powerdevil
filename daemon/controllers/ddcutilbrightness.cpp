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
    DDCutilDisplay *display(const QString &id) const;
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    void displayStatusChanged(DDCA_Display_Status_Event &event);
#endif
Q_SIGNALS:
    void displayAdded();
    void displayRemoved(QString path);
    void dpmsStateChanged(QString path, bool isSleeping);
    void brightnessChanged(int brightness, int maxBrightness);

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

DDCutilPrivateSingleton &DDCutilPrivateSingleton::instance()
{
    static DDCutilPrivateSingleton singleton;
    return singleton;
}

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

void DDCutilPrivateSingleton::detect()
{
    if (qEnvironmentVariableIntValue("POWERDEVIL_NO_DDCUTIL") > 0) {
        return;
    }

    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Check for monitors using ddca_get_display_refs()...";
    // Inquire about detected monitors.
    DDCA_Display_Ref *displayRefs = nullptr;
    ddca_get_display_refs(false, &displayRefs);

    int displayCount = 0;
    while (displayRefs[displayCount] != nullptr) {
        ++displayCount;
    }
    qCInfo(POWERDEVIL) << "[DDCutilBrightness]:" << displayCount << "display(s) were detected";

    for (int i = 0; i < displayCount; ++i) {
        auto display = std::make_unique<DDCutilDisplay>(displayRefs[i]);

        QString displayId = generateDisplayId(display->ioPath());
        if (displayId.isEmpty()) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: Cannot generate ID for display with model name:" << display->label() << "- ignoring";
            continue;
        }
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Created ID:" << displayId << "for display:" << display->label();

        if (!display->supportsBrightness()) {
            qCInfo(POWERDEVIL) << "[DDCutilBrightness]: Display" << display->label() << "does not seem to support brightness control - ignoring";
            continue;
        }

        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: Display supports Brightness, adding handle to list";
        m_displays[displayId] = std::move(display);
        m_displayIds += displayId;
    }

    if (!m_displayIds.isEmpty()) {
        connect(m_displays.at(m_displayIds.first()).get(), &DDCutilDisplay::brightnessChanged, this, &DDCutilPrivateSingleton::brightnessChanged);
    }
    for (const QString &displayId : std::as_const(m_displayIds)) {
        DDCutilDisplay *display = m_displays.at(displayId).get();
        connect(display, &DDCutilDisplay::supportsBrightnessChanged, this, [this, displayId](bool isSupported) {
            if (!isSupported) {
                removeDisplay(displayId);
            }
        });
    }
}

QStringList DDCutilPrivateSingleton::displayIds() const
{
    return m_displayIds;
}

DDCutilDisplay *DDCutilPrivateSingleton::display(const QString &id) const
{
    return m_displayIds.contains(id) ? m_displays.at(id).get() : nullptr;
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
    if (auto index = m_displayIds.indexOf(path); index != -1) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: removing display" << path;
        m_displayIds.remove(index);
        m_displays.erase(path);

        if (index == 0 && !m_displayIds.empty()) {
            connect(m_displays.at(m_displayIds.first()).get(), &DDCutilDisplay::brightnessChanged, this, &DDCutilPrivateSingleton::brightnessChanged);
        }
    } else {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: failed to remove display";
    }
}

void DDCutilPrivateSingleton::setDpmsState(const QString &path, bool isSleeping)
{
#if DDCUTIL_VERSION >= QT_VERSION_CHECK(2, 1, 0)
    if (m_displayIds.contains(path)) {
        if (isSleeping) {
            m_displays.at(path)->pauseWorker();
            qCDebug(POWERDEVIL) << "[DDCutilBrightness]: set display to sleep:" << path;
        } else {
            m_displays.at(path)->resumeWorker();
            qCDebug(POWERDEVIL) << "[DDCutilBrightness]: wakeup display:" << path;
        }
    }
#endif
}
#endif

DDCutilBrightness::DDCutilBrightness(QObject *parent)
    : QObject(parent)
{
#ifdef WITH_DDCUTIL
    connect(&DDCutilPrivateSingleton::instance(), &DDCutilPrivateSingleton::brightnessChanged, this, &DDCutilBrightness::brightnessChanged);
#endif
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
    if (DDCutilDisplay *display = DDCutilPrivateSingleton::instance().display(displayId)) {
        return display->brightness();
    }
#endif
    return -1;
}

int DDCutilBrightness::maxBrightness(const QString &displayId)
{
#ifdef WITH_DDCUTIL
    if (DDCutilDisplay *display = DDCutilPrivateSingleton::instance().display(displayId)) {
        return display->maxBrightness();
    }
#endif
    return -1;
}

void DDCutilBrightness::setBrightness(const QString &displayId, int value)
{
#ifdef WITH_DDCUTIL
    if (DDCutilDisplay *display = DDCutilPrivateSingleton::instance().display(displayId)) {
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: setBrightness: displayId:" << displayId << "brightness:" << value;
        display->setBrightness(value);
    }
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
