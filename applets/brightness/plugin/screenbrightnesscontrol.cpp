/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnesscontrol.h"

#include <brightnesscontrolplugin_debug.h>

#include <QCoroDBusPendingCall>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QPointer>
#include <QScopeGuard>

using namespace Qt::StringLiterals;

namespace
{
static const QString SCREENBRIGHTNESS_SERVICE = u"org.kde.ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_PATH = u"/org/kde/ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_IFACE = u"org.kde.ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_DISPLAY_PATH_TEMPLATE = u"/org/kde/ScreenBrightness/%1"_s;
static const QString SCREENBRIGHTNESS_DISPLAY_IFACE = u"org.kde.ScreenBrightness.Display"_s;
static const QString DBUS_PROPERTIES_IFACE = u"org.freedesktop.DBus.Properties"_s;
}

ScreenBrightnessControl::ScreenBrightnessControl(QObject *parent)
    : QObject(parent)
{
    static uint pluginId = 0;
    ++pluginId;
    m_alreadyChangedContext = QStringLiteral("AlreadyChanged-%1").arg(pluginId);

    m_serviceWatcher = std::make_unique<QDBusServiceWatcher>(SCREENBRIGHTNESS_SERVICE,
                                                             QDBusConnection::sessionBus(),
                                                             QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &ScreenBrightnessControl::onServiceRegistered);
    connect(m_serviceWatcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &ScreenBrightnessControl::onServiceUnregistered);

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(SCREENBRIGHTNESS_SERVICE)) {
        onServiceRegistered();
    } else {
        qCWarning(APPLETS::BRIGHTNESS) << "D-Bus service not available:" << SCREENBRIGHTNESS_SERVICE;
    }
}

ScreenBrightnessControl::~ScreenBrightnessControl()
{
}

ScreenBrightnessDisplayModel *ScreenBrightnessControl::displays()
{
    return &m_displays;
}

const ScreenBrightnessDisplayModel *ScreenBrightnessControl::displays() const
{
    return &m_displays;
}

void ScreenBrightnessControl::adjustBrightnessRatio(double delta)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"AdjustBrightnessRatio"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << delta << flags;
    QDBusConnection::sessionBus().asyncCall(msg);
}

void ScreenBrightnessControl::adjustBrightnessStep(ScreenBrightnessControl::StepAction stepAction)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"AdjustBrightnessStep"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << qToUnderlying(stepAction) << flags;
    QDBusConnection::sessionBus().asyncCall(msg);
}

void ScreenBrightnessControl::setBrightness(const QString &displayName, int value)
{
    QVariant oldBrightness = m_displays.data(m_displays.displayIndex(displayName), ScreenBrightnessDisplayModel::BrightnessRole);

    if (oldBrightness == value) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE,
                                                      SCREENBRIGHTNESS_DISPLAY_PATH_TEMPLATE.arg(displayName),
                                                      SCREENBRIGHTNESS_DISPLAY_IFACE,
                                                      u"SetBrightnessWithContext"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << value << flags << m_alreadyChangedContext;
    QDBusPendingCall async = QDBusConnection::sessionBus().asyncCall(msg);
    m_brightnessChangeWatcher.reset(new QDBusPendingCallWatcher(async));

    connect(m_brightnessChangeWatcher.get(),
            &QDBusPendingCallWatcher::finished,
            this,
            [this, displayName, oldValue = oldBrightness.toInt()](QDBusPendingCallWatcher *watcher) {
                const QDBusPendingReply<void> reply = *watcher;
                if (reply.isError()) {
                    qCWarning(APPLETS::BRIGHTNESS) << "error setting brightness via dbus" << reply.error();
                    m_displays.onBrightnessChanged(displayName, oldValue);
                }
                m_brightnessChangeWatcher.reset();
            });

    m_displays.onBrightnessChanged(displayName, value);
}

void ScreenBrightnessControl::onGlobalPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    const QString displayNamesKey = u"DisplaysDBusNames"_s;

    if (ifaceName == SCREENBRIGHTNESS_IFACE) {
        if (changedProps.contains(displayNamesKey) || invalidatedProps.contains(displayNamesKey)) {
            queryAndUpdateDisplays();
        }
    }
}

void ScreenBrightnessControl::onBrightnessChanged(const QString &displayName, int value, const QString &sourceClientName, const QString &sourceClientContext)
{
    if (sourceClientName == QDBusConnection::sessionBus().baseService() && sourceClientContext == m_alreadyChangedContext) {
        qCDebug(APPLETS::BRIGHTNESS) << "ignoring brightness change, it's coming from the applet itself";
        return;
    }
    m_displays.onBrightnessChanged(displayName, value);
}

void ScreenBrightnessControl::onBrightnessRangeChanged(const QString &displayName, int max, int value)
{
    m_displays.onBrightnessRangeChanged(displayName, max, value);

    QVariant firstDisplayMax = m_displays.data(m_displays.index(0, 0), ScreenBrightnessDisplayModel::MaxBrightnessRole);

    m_isBrightnessAvailable = firstDisplayMax.isValid() && firstDisplayMax.toInt() > 0;
}

QCoro::Task<void> ScreenBrightnessControl::onServiceRegistered()
{
    m_serviceRegistered = true;
    QPointer<ScreenBrightnessControl> alive{this};

    if (!QDBusConnection::sessionBus().connect(SCREENBRIGHTNESS_SERVICE,
                                               SCREENBRIGHTNESS_PATH,
                                               DBUS_PROPERTIES_IFACE,
                                               u"PropertiesChanged"_s,
                                               this,
                                               SLOT(onGlobalPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to property changes via dbus";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SCREENBRIGHTNESS_SERVICE,
                                               SCREENBRIGHTNESS_PATH,
                                               SCREENBRIGHTNESS_IFACE,
                                               u"BrightnessChanged"_s,
                                               this,
                                               SLOT(onBrightnessChanged(QString, int, QString, QString)))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to Brightness changes via dbus";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SCREENBRIGHTNESS_SERVICE,
                                               SCREENBRIGHTNESS_PATH,
                                               SCREENBRIGHTNESS_IFACE,
                                               u"BrightnessRangeChanged"_s,
                                               this,
                                               SLOT(onBrightnessRangeChanged(QString, int, int)))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to brightness range changes via dbus";
        co_return;
    }

    if (!co_await queryAndUpdateDisplays()) {
        qCWarning(APPLETS::BRIGHTNESS) << "error fetching display names via dbus";
        co_return;
    }

    if (!alive || !m_serviceRegistered) {
        qCWarning(APPLETS::BRIGHTNESS) << "ScreenBrightnessControl destroyed during initialization, or service got unregistered. Returning early";
        co_return;
    }

    m_isBrightnessAvailable = true;
}

void ScreenBrightnessControl::onServiceUnregistered()
{
    m_serviceRegistered = false;

    QDBusConnection::sessionBus().disconnect(SCREENBRIGHTNESS_SERVICE,
                                             SCREENBRIGHTNESS_PATH,
                                             DBUS_PROPERTIES_IFACE,
                                             u"PropertiesChanged"_s,
                                             this,
                                             SLOT(onGlobalPropertiesChanged(QString, QVariantMap, QStringList)));

    QDBusConnection::sessionBus().disconnect(SCREENBRIGHTNESS_SERVICE,
                                             SCREENBRIGHTNESS_PATH,
                                             SCREENBRIGHTNESS_IFACE,
                                             u"BrightnessChanged"_s,
                                             this,
                                             SLOT(onBrightnessChanged(QString, int, QString, QString)));

    QDBusConnection::sessionBus().disconnect(SCREENBRIGHTNESS_SERVICE,
                                             SCREENBRIGHTNESS_PATH,
                                             SCREENBRIGHTNESS_IFACE,
                                             u"BrightnessRangeChanged"_s,
                                             this,
                                             SLOT(onBrightnessRangeChanged(QString, int, int)));

    m_displays.setKnownDisplayNames({});
    m_isBrightnessAvailable = false;
}

QCoro::Task<bool> ScreenBrightnessControl::queryAndUpdateDisplays()
{
    m_shouldRecheckDisplays = true;
    if (m_displaysUpdating) {
        // With m_shouldRecheckDisplays == true, the loop below will be repeated
        // by the already running function once it's done with its current update.
        co_return false;
    }
    QPointer<ScreenBrightnessControl> alive{this};

    while (m_shouldRecheckDisplays) {
        m_shouldRecheckDisplays = false; // can be set to true while waiting for async calls
        m_displaysUpdating = true;
        QScopeGuard whenDone([this, alive] {
            if (alive) {
                m_displaysUpdating = false;
            }
        });

        QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, DBUS_PROPERTIES_IFACE, u"Get"_s);
        msg << SCREENBRIGHTNESS_IFACE << u"DisplaysDBusNames"_s;

        const QDBusReply<QVariant> reply = co_await QDBusConnection::sessionBus().asyncCall(msg);
        if (!alive || !reply.isValid() || !m_serviceRegistered) {
            qCWarning(APPLETS::BRIGHTNESS) << "error getting display ids via dbus:" << reply.error();
            co_return false;
        }
        const QStringList displayNames = reply.value().value<QStringList>();

        m_displays.setKnownDisplayNames(displayNames);

        for (const QString &displayName : m_displays.knownDisplayNamesWithMissingData()) {
            co_await queryAndInsertDisplayData(displayName);
            if (!alive || !m_serviceRegistered) {
                qCWarning(APPLETS::BRIGHTNESS) << "ScreenBrightnessControl destroyed while querying displays, returning early";
                co_return false;
            }
        }
    }
    co_return true;
}

QCoro::Task<void> ScreenBrightnessControl::queryAndInsertDisplayData(const QString &displayName)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE,
                                                      SCREENBRIGHTNESS_DISPLAY_PATH_TEMPLATE.arg(displayName),
                                                      DBUS_PROPERTIES_IFACE,
                                                      QStringLiteral("GetAll"));
    msg << SCREENBRIGHTNESS_DISPLAY_IFACE;
    QPointer<ScreenBrightnessControl> alive{this};
    const QDBusReply<QVariantMap> reply = co_await QDBusConnection::sessionBus().asyncCall(msg);
    if (!alive || !reply.isValid() || !m_serviceRegistered) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting display properties via dbus:" << reply.error();
        co_return;
    }
    const QVariantMap &props = reply.value();

    auto label = props.value(u"Label"_s).value<QString>();
    if (label.isEmpty()) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting display label via dbus: property missing";
        co_return;
    }

    if (!props.contains(u"IsInternal"_s)) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting display is-internal via dbus: property missing";
        co_return;
    }
    auto isInternal = props.value(u"IsInternal"_s).value<bool>();

    if (!props.contains(u"Brightness"_s)) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting display brightness via dbus: property missing";
        co_return;
    }
    auto brightness = props.value(u"Brightness"_s).value<int>();

    auto maxBrightness = props.value(u"MaxBrightness"_s).value<int>();
    if (maxBrightness <= 0) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting max display brightness via dbus: property missing";
        co_return;
    }

    m_displays.setDisplayData(displayName, label, isInternal, brightness, maxBrightness);
}

QBindable<bool> ScreenBrightnessControl::bindableIsBrightnessAvailable()
{
    return &m_isBrightnessAvailable;
}

#include "moc_screenbrightnesscontrol.cpp"
