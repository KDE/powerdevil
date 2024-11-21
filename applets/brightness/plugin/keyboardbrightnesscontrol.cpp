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

QCoro::Task<void> KeyboardBrightnessControl::onServiceRegistered()
{
    m_serviceRegistered = true;

    QDBusMessage brightnessMax = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                                                u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                                                u"keyboardBrightnessMax"_s);
    QPointer<KeyboardBrightnessControl> alive{this};
    const QDBusReply<int> brightnessMaxReply = co_await QDBusConnection::sessionBus().asyncCall(brightnessMax);
    if (!alive || !brightnessMaxReply.isValid() || !m_serviceRegistered) {
        qCWarning(APPLETS::BRIGHTNESS) << "error getting max keyboard brightness via dbus" << brightnessMaxReply.error();
        co_return;
    }
    m_maxBrightness = brightnessMaxReply.value();

    QDBusMessage brightness = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                             u"/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"_s,
                                                             u"org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"_s,
                                                             u"keyboardBrightness"_s);
    const QDBusReply<int> brightnessReply = co_await QDBusConnection::sessionBus().asyncCall(brightness);
    if (!alive || !brightnessReply.isValid() || !m_serviceRegistered) {
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

void KeyboardBrightnessControl::onServiceUnregistered()
{
    m_serviceRegistered = false;

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

    m_isBrightnessAvailable = false;
}

#include "moc_keyboardbrightnesscontrol.cpp"
