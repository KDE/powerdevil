/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "backlightbrightness_linux.h"

#include <powerdevil_backlightbrightness_debug.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDebug>
#include <QTimer>
#include <QVariantAnimation>

#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KAuth/ExecuteJob>
#include <KAuth/HelperSupport>
#include <KLocalizedString>

#include <algorithm> // std::max, std::min
#include <limits> // std::numeric_limits
#include <memory> // std::shared_ptr

#include "udevqt.h"

using namespace Qt::StringLiterals;

inline constexpr QLatin1StringView SYSFS_BACKLIGHT_SUBSYSTEM("backlight");
inline constexpr QLatin1StringView SYSFS_LED_SUBSYSTEM("leds");

inline constexpr QLatin1StringView LOGIN1_SERVICE("org.freedesktop.login1");
inline constexpr QLatin1StringView LOGIN1_SESSION_PATH("/org/freedesktop/login1/session/auto");
inline constexpr QLatin1StringView LOGIN1_SESSION_IFACE("org.freedesktop.login1.Session");

BacklightDetector::BacklightDetector(QObject *parent)
    : DisplayBrightnessDetector(parent)
{
}

void BacklightDetector::detect()
{
    if (m_display) {
        disconnect(m_display.get(), nullptr, this, nullptr);
    }
    std::shared_ptr<BacklightBrightness> deleteAfterDetectionFinished(m_display.release());

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        qCWarning(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "not supported: missing D-Bus service" << LOGIN1_SERVICE;
    } else if (QList<BacklightSysfsDevice> devices = BacklightSysfsDevice::getBacklightTypeDevices(); !devices.isEmpty()) {
        m_display.reset(new BacklightBrightness(std::move(devices), this));
    } else {
        qCInfo(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "not supported: no kernel backlight interface found";
    }
    Q_EMIT detectionFinished(m_display != nullptr);
}

QList<DisplayBrightness *> BacklightDetector::displays() const
{
    return m_display ? QList<DisplayBrightness *>{m_display.get()} : QList<DisplayBrightness *>{};
}

BacklightBrightness::BacklightBrightness(QList<BacklightSysfsDevice> devices, QObject *parent)
    : DisplayBrightness(parent)
    , m_devices(std::move(devices))
{
    if (m_devices.isEmpty()) {
        return;
    }
    m_observedBrightness = m_devices.constFirst().readBrightness().value_or(-1);
    m_requestedBrightness = m_observedBrightness;
    m_executedBrightness = m_observedBrightness;

    // Kernel doesn't send uevent for leds-class devices, or at least that's what commit 26a48f9db
    // claimed (although the monitored subsystem was already hardcoded to "backlight" then).
    // Keep track of backlight devices for changes not initiated by this class.
    if (m_devices.constFirst().subsystem == SYSFS_BACKLIGHT_SUBSYSTEM) {
        UdevQt::Client *client = new UdevQt::Client(QStringList(SYSFS_BACKLIGHT_SUBSYSTEM), this);
        connect(client, &UdevQt::Client::deviceChanged, this, &BacklightBrightness::onDeviceChanged);
    }
}

bool BacklightBrightness::isSupported() const
{
    return !m_devices.isEmpty();
}

void BacklightBrightness::onDeviceChanged(const UdevQt::Device &device)
{
    Q_ASSERT(!m_devices.isEmpty());

    if (device.name() != m_devices.constFirst().interface) {
        return;
    }
    if (device.sysfsProperty("max_brightness"_L1).toInt() != m_devices.constFirst().maxBrightness) {
        return;
    }
    std::optional<int> newBrightness = m_devices.constFirst().readBrightness();
    if (!newBrightness.has_value() || m_observedBrightness == *newBrightness) {
        return;
    }
    int lastObservedBrightness = m_observedBrightness;
    m_observedBrightness = *newBrightness;

    qCDebug(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "Udev device changed brightness to:" << m_observedBrightness << "for" << device.sysfsPath();

    //
    // Ignore any brightness changes that we initiated in this class, send signals only for external changes

    if (m_observedBrightness > lastObservedBrightness) {
        // Moving up means we don't need a lower boundary, the only valid animation targets are
        // executed brightness (current target) and expected max brightness (delayed previous target)
        if (m_executedBrightness >= m_observedBrightness) {
            m_expectedMinBrightness = -1;
            return;
        } else if (m_expectedMaxBrightness != -1 && m_expectedMaxBrightness >= m_observedBrightness) {
            return;
        }
    } else {
        // Moving down means we don't need a upper boundary, the only valid animation targets are
        // executed brightness (current target) and expected min brightness (delayed previous target)
        if (m_executedBrightness <= m_observedBrightness) {
            m_expectedMaxBrightness = -1;
            return;
        } else if (m_expectedMinBrightness != -1 && m_expectedMinBrightness <= m_observedBrightness) {
            return;
        }
    }

    qCDebug(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "External brightness change observed:" << m_observedBrightness << "/" << m_devices.constFirst().maxBrightness;
    m_requestedBrightness = m_observedBrightness;
    m_executedBrightness = m_observedBrightness;
    m_expectedMinBrightness = -1;
    m_expectedMaxBrightness = -1;
    Q_EMIT externalBrightnessChangeObserved(this, m_observedBrightness);
}

QString BacklightBrightness::id() const
{
    // BacklightDetector only ever owns one BacklightBrightness object, so this is necessarily unique
    return u"display"_s;
}

QString BacklightBrightness::label() const
{
    return i18nc("Display label", "Built-in Screen");
}

int BacklightBrightness::knownSafeMinBrightness() const
{
    // Some laptop displays have been known to turn off completely when set to 0.
    // We could keep lists of model names for displays where this is the case, but that's a lot of
    // work and still can't guarantee that we won't be wrong with future displays. Use 1 as the
    // lowest value that we're actually sure won't turn off the display.
    return 1;
}

int BacklightBrightness::maxBrightness() const
{
    return m_devices.isEmpty() ? -1 : m_devices.constFirst().maxBrightness;
}

int BacklightBrightness::brightness() const
{
    return m_requestedBrightness;
}

void BacklightBrightness::setBrightness(int newBrightness, bool allowAnimations)
{
    if (!isSupported()) {
        qCWarning(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "Not supported, setBrightness() should not be called";
        return;
    }
    if (newBrightness < 0 || newBrightness > maxBrightness()) {
        qCWarning(POWERDEVIL_BACKLIGHTBRIGHTNESS) << "Invalid brightness requested:" << newBrightness << "- ignoring | valid range: 0 to" << maxBrightness();
        return;
    }
    m_requestedBrightness = newBrightness;
    m_requestedAllowAnimations = allowAnimations;

    if (m_isWaitingForAsyncIPC) {
        // There's already an async SetBrightness D-Bus call running. Don't start a second one
        // until this one has replied. (Note that even if we cancel an animation now, previous
        // calls may still result in a handful of onDeviceChanged() events later.)
        // Remember m_requestedBrightness for later and handle it in the result handler lambda.
        return;
    }

    const int oldExecutedBrightness = m_executedBrightness;
    m_executedBrightness = newBrightness;

    // Define the range of expected animation values, if we observe any values outside of this
    // then we'll emit a brightness change signal as "external change"
    if (newBrightness > oldExecutedBrightness && m_expectedMinBrightness == -1) {
        m_expectedMinBrightness = qMin(m_observedBrightness, oldExecutedBrightness);
    }
    if (newBrightness < oldExecutedBrightness && m_expectedMaxBrightness == -1) {
        m_expectedMaxBrightness = qMax(m_observedBrightness, oldExecutedBrightness);
    }

    // Make sure there are enough integer steps of difference to run a smooth animation
    const int brightnessDiff = qAbs(m_observedBrightness - newBrightness);
    const bool willAnimate = brightnessDiff >= m_brightnessAnimationThreshold && allowAnimations && m_brightnessAnimationDurationMsec > 0;

    if (willAnimate) {
        animateBrightnessWithLogin1(newBrightness);
    } else {
        cancelBrightnessAnimation();
        setBrightnessWithLogin1(newBrightness);
    }
}

void BacklightBrightness::cancelBrightnessAnimation()
{
    if (m_brightnessAnimation) {
        m_brightnessAnimation->stop();
        disconnect(m_brightnessAnimation, nullptr, nullptr, nullptr);
    }
}

void BacklightBrightness::animateBrightnessWithLogin1(int newBrightness)
{
    cancelBrightnessAnimation();

    if (!m_brightnessAnimation) {
        m_brightnessAnimation = new QVariantAnimation(this);
    }
    m_brightnessAnimation->setDuration(m_brightnessAnimationDurationMsec);

    connect(m_brightnessAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        // When animating to zero, it emits a value change to 0 before starting the animation...
        if (m_brightnessAnimation->state() == QAbstractAnimation::Running) {
            const bool isFinalAnimationValue = value == m_brightnessAnimation->endValue();
            setBrightnessWithLogin1(value.toInt(), isFinalAnimationValue);
        }
    });

    int currentBrightness = m_devices.constFirst().readBrightness().value_or(m_observedBrightness);

    m_brightnessAnimation->setStartValue(currentBrightness);
    m_brightnessAnimation->setEndValue(newBrightness);
    m_brightnessAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    m_brightnessAnimation->start();
}

void BacklightBrightness::setBrightnessWithLogin1(int newBrightness, bool isFinalAnimationValue)
{
    Q_ASSERT(!m_devices.isEmpty());

    const int firstMaxBrightness = std::max(1, m_devices.constFirst().maxBrightness);
    for (qsizetype i = 0; i < m_devices.size(); ++i) {
        const BacklightSysfsDevice &device = m_devices[i];
        bool isFirstDevice = i == 0;
        unsigned int perDeviceBrightness = newBrightness;

        if (!isFirstDevice) {
            // Some monitor brightness values are ridiculously high, and can easily overflow during computation
            const qint64 newBrightness64 =
                static_cast<qint64>(newBrightness) * static_cast<qint64>(device.maxBrightness) / static_cast<qint64>(firstMaxBrightness);
            // cautiously truncate it back
            perDeviceBrightness = static_cast<unsigned int>(std::min(static_cast<qint64>(std::numeric_limits<int>::max()), newBrightness64));
        }

        if (isFirstDevice) {
            m_isWaitingForAsyncIPC = true;
        }

        QDBusMessage msg = QDBusMessage::createMethodCall(LOGIN1_SERVICE, LOGIN1_SESSION_PATH, LOGIN1_SESSION_IFACE, "SetBrightness"_L1);
        msg << device.subsystem << device.interface << perDeviceBrightness;

        QDBusPendingCall call = QDBusConnection::systemBus().asyncCall(msg);

        if (isFirstDevice) {
            QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(call, this);

            connect(callWatcher, &QDBusPendingCallWatcher::finished, this, [this, isFinalAnimationValue](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();

                Q_ASSERT(m_isWaitingForAsyncIPC);

                if (reply.isError()) {
                    m_isWaitingForAsyncIPC = false;
                    qCWarning(POWERDEVIL_BACKLIGHTBRIGHTNESS)
                        << "Failed to set screen brightness via" << LOGIN1_SERVICE << "D-Bus service:" << reply.error().message();
                    cancelBrightnessAnimation();
                    std::optional<int> currentBrightness = m_devices.constFirst().readBrightness();
                    if (currentBrightness.has_value()) {
                        m_observedBrightness = *currentBrightness;
                    }
                    Q_EMIT externalBrightnessChangeObserved(this, m_observedBrightness);
                    return;
                }

                if (isFinalAnimationValue) {
                    m_isWaitingForAsyncIPC = false;
                }
                if (isFinalAnimationValue && m_requestedBrightness != m_executedBrightness) {
                    // We had another setBrightness() request come in in the meantime:
                    // apply it now that m_isWaitingForAsyncIPC is not blocking the call
                    setBrightness(m_requestedBrightness, m_requestedAllowAnimations);
                }
            });
        }
    }
}

bool BacklightBrightness::isInternal() const
{
    return true;
}

#include "moc_backlightbrightness_linux.cpp"
