/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "keyboardbrightnesscontrol.h"

#include <brightnesscontrolplugin_debug.h>

#include <QCoroDBusPendingCall>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1String SOLID_POWERMANAGEMENT_SERVICE("org.kde.Solid.PowerManagement");
constexpr QLatin1String KEYBOARD_BRIGHTNESS_ACTION("KeyboardBrightnessControl");
}

KeyboardBrightnessControl::KeyboardBrightnessControl(QObject *parent)
    : QObject(parent)
{
    m_serviceWatcher = std::make_unique<QDBusServiceWatcher>(SOLID_POWERMANAGEMENT_SERVICE,
                                                             QDBusConnection::sessionBus(),
                                                             QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher.get(), &QDBusServiceWatcher::serviceRegistered, this, &KeyboardBrightnessControl::onServiceRegistered);
    connect(m_serviceWatcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &KeyboardBrightnessControl::onServiceUnregistered);

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(SOLID_POWERMANAGEMENT_SERVICE)) {
        onServiceRegistered();
    } else {
        qCWarning(APPLETS::BRIGHTNESS) << "D-Bus service not available:" << SOLID_POWERMANAGEMENT_SERVICE;
    }
}

KeyboardBrightnessControl::~KeyboardBrightnessControl()
{
}

void KeyboardBrightnessControl::setBrightness(int value)
{
    m_brightness = value;

    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                      u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                                      u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                                      m_isSilent ? u"setKeyboardBrightnessSilent"_s : u"setKeyboardBrightness"_s);
    msg << value;
    QDBusConnection::sessionBus().asyncCall(msg);
}

QBindable<bool> KeyboardBrightnessControl::bindableIsBrightnessAvailable()
{
    return &m_isBrightnessAvailable;
}

QBindable<int> KeyboardBrightnessControl::bindableBrightness()
{
    return &m_brightness;
}

QBindable<int> KeyboardBrightnessControl::bindableBrightnessMax()
{
    return &m_maxBrightness;
}

void KeyboardBrightnessControl::setIsSilent(bool silent)
{
    m_isSilent = silent;
}

void KeyboardBrightnessControl::onBrightnessChanged(int value)
{
    m_brightness = value;
}

void KeyboardBrightnessControl::onBrightnessMaxChanged(int value)
{
    m_maxBrightness = value;

    m_isBrightnessAvailable = value > 0;
}

void KeyboardBrightnessControl::onServiceRegistered()
{
    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement"_s,
                                               u"org.kde.Solid.PowerManagement"_s,
                                               u"supportedActionsChanged"_s,
                                               this,
                                               SLOT(onSupportedActionsChanged()))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to supported action changes via dbus";
    }

    onSupportedActionsChanged();
}

void KeyboardBrightnessControl::onServiceUnregistered()
{
    onActionUnsupported();

    QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                             u"/org/kde/Solid/PowerManagement"_s,
                                             u"org.kde.Solid.PowerManagement"_s,
                                             u"supportedActionsChanged"_s,
                                             this,
                                             SLOT(onSupportedActionsChanged()));
}

QCoro::Task<bool> KeyboardBrightnessControl::isActionSupported(const QString &actionName)
{
    QDBusMessage checkActionMsg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                                 u"/org/kde/Solid/PowerManagement"_s,
                                                                 u"org.kde.Solid.PowerManagement"_s,
                                                                 u"isActionSupported"_s);
    checkActionMsg << actionName;

    const QDBusReply<bool> checkActionReply = co_await QDBusConnection::sessionBus().asyncCall(checkActionMsg);

    if (!checkActionReply.isValid()) {
        qCWarning(APPLETS::BRIGHTNESS) << "error retrieving action status for" << actionName << checkActionReply.error();
        co_return false;
    }
    co_return checkActionReply.value();
}

QCoro::Task<void> KeyboardBrightnessControl::onSupportedActionsChanged()
{
    QPointer<KeyboardBrightnessControl> alive{this};
    const bool actionSupported = co_await isActionSupported(KEYBOARD_BRIGHTNESS_ACTION);
    if (!alive) {
        co_return;
    }

    if (actionSupported) {
        onActionSupported();
    } else {
        qCInfo(APPLETS::BRIGHTNESS) << "D-Bus action" << KEYBOARD_BRIGHTNESS_ACTION << "is not available at service" << SOLID_POWERMANAGEMENT_SERVICE;
        onActionUnsupported();
    }
}

QCoro::Task<void> KeyboardBrightnessControl::onActionSupported()
{
    if (m_initialized) {
        co_return;
    }
    m_initialized = true;

    QDBusMessage brightnessMax = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                                                u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                                                u"keyboardBrightnessMax"_s);
    QPointer<KeyboardBrightnessControl> alive{this};
    const QDBusReply<int> brightnessMaxReply = co_await QDBusConnection::sessionBus().asyncCall(brightnessMax);
    if (!alive || !brightnessMaxReply.isValid() || !m_initialized) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting max keyboard brightness via dbus" << brightnessMaxReply.error();
        co_return;
    }
    m_maxBrightness = brightnessMaxReply.value();

    QDBusMessage brightness = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                             u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                                             u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                                             u"keyboardBrightness"_s);
    const QDBusReply<int> brightnessReply = co_await QDBusConnection::sessionBus().asyncCall(brightness);
    if (!alive || !brightnessReply.isValid() || !m_initialized) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting keyboard brightness via dbus" << brightnessReply.error();
        co_return;
    }
    m_brightness = brightnessReply.value();

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                               u"keyboardBrightnessChanged"_s,
                                               this,
                                               SLOT(onBrightnessChanged(int)))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to Keyboard Brightness changes via dbus";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                               u"keyboardBrightnessMaxChanged"_s,
                                               this,
                                               SLOT(onBrightnessMaxChanged(int)))) {
        qCWarning(APPLETS::BRIGHTNESS) << "error connecting to max keyboard Brightness changes via dbus";
        co_return;
    }

    m_isBrightnessAvailable = true;
}

void KeyboardBrightnessControl::onActionUnsupported()
{
    m_isBrightnessAvailable = false;

    if (!m_initialized) {
        return;
    }
    m_initialized = false;

    QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                             u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                             u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                             u"keyboardBrightnessChanged"_s,
                                             this,
                                             SLOT(onBrightnessChanged(int)));

    QDBusConnection::sessionBus().disconnect(SOLID_POWERMANAGEMENT_SERVICE,
                                             u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                             u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                             u"keyboardBrightnessMaxChanged"_s,
                                             this,
                                             SLOT(onBrightnessMaxChanged(int)));
}

#include "moc_keyboardbrightnesscontrol.cpp"
