/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "powerdevilupowerbackend.h"

#include <PowerDevilSettings.h>
#include <powerdevil_debug.h>

#include <QDBusMessage>
#include <QDebug>
#include <QPropertyAnimation>
#include <QTextStream>
#include <QTimer>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kauth_version.h>

#include "ddcutilbrightness.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject *parent)
    : BackendInterface(parent)
    , m_cachedScreenBrightness(0)
    , m_cachedKeyboardBrightness(0)
    , m_upowerInterface(nullptr)
    , m_kbdBacklight(nullptr)
    , m_kbdMaxBrightness(0)
    , m_lidIsPresent(false)
    , m_lidIsClosed(false)
    , m_isLedBrightnessControl(false)
{
}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend() = default;

void PowerDevilUPowerBackend::init()
{
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(UPOWER_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(UPOWER_SERVICE);
    }

    connect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);
    m_upowerInterface = new OrgFreedesktopUPowerInterface(UPOWER_SERVICE, "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);
    m_ddcBrightnessControl = new DDCutilBrightness();

    qCDebug(POWERDEVIL) << "Trying Backlight Helper first...";
    KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
    brightnessAction.setHelperId(HELPER_ID);
    KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
    connect(brightnessJob, &KJob::result, this, [this, brightnessJob] {
        if (brightnessJob->error()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
            qCDebug(POWERDEVIL) << brightnessJob->errorText();
            Q_EMIT brightnessSupportQueried(false);
            return;
        }
        m_cachedScreenBrightness = brightnessJob->data()["brightness"].toFloat();

        KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
        brightnessMaxAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
        connect(brightnessMaxJob, &KJob::result, this, [this, brightnessMaxJob] {
            if (brightnessMaxJob->error()) {
                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
                qCDebug(POWERDEVIL) << brightnessMaxJob->errorText();
            } else {
                m_brightnessMax = brightnessMaxJob->data()["brightnessmax"].toInt();
            }

#ifdef Q_OS_FREEBSD
            // FreeBSD doesn't have the sysfs interface that the bits below expect;
            // the sysfs calls always fail, leading to brightnessSupportQueried(false) emission.
            // Skip that command and carry on with the information that we do have.
            Q_EMIT brightnessSupportQueried(m_brightnessMax > 0);
#else
            KAuth::Action syspathAction("org.kde.powerdevil.backlighthelper.syspath");
            syspathAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob* syspathJob = syspathAction.execute();
            connect(syspathJob, &KJob::result, this, [this, syspathJob] {
                if (syspathJob->error()) {
                    qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.syspath failed";
                    qCDebug(POWERDEVIL) << syspathJob->errorText();
                    Q_EMIT brightnessSupportQueried(false);
                    return;
                }
                m_syspath = syspathJob->data()["syspath"].toString();
                m_syspath = QFileInfo(m_syspath).symLinkTarget();

                m_isLedBrightnessControl = m_syspath.contains(QLatin1String("/leds/"));
                if (!m_isLedBrightnessControl) {
                    UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
                    connect(client, &UdevQt::Client::deviceChanged, this, &PowerDevilUPowerBackend::onDeviceChanged);
                }

                Q_EMIT brightnessSupportQueried(m_brightnessMax > 0);
            });
            syspathJob->start();
#endif
        });
        brightnessMaxJob->start();
    });
    brightnessJob->start();
}

void PowerDevilUPowerBackend::initWithBrightness(bool screenBrightnessAvailable)
{
    if (!screenBrightnessAvailable) {
        qCDebug(POWERDEVIL) << "Brightness Helper have failed. Trying DDC Helper for brightness controls...";
        m_ddcBrightnessControl->detect();
        if (m_ddcBrightnessControl->isSupported()) {
            qCDebug(POWERDEVIL) << "Using DDCutillib";
            m_cachedScreenBrightness = screenBrightness();
            const int duration = PowerDevilSettings::brightnessAnimationDuration();
            if (duration > 0 && screenBrightnessMax() >= PowerDevilSettings::brightnessAnimationThreshold()) {
                m_brightnessAnimation = new QPropertyAnimation(this);
                m_brightnessAnimation->setTargetObject(this);
                m_brightnessAnimation->setDuration(duration);
                connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
                connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &PowerDevilUPowerBackend::slotScreenBrightnessChanged);
            }
            screenBrightnessAvailable = true;
        }
    }

    disconnect(this, &PowerDevilUPowerBackend::brightnessSupportQueried, this, &PowerDevilUPowerBackend::initWithBrightness);

    QDBusConnection::systemBus().connect(UPOWER_SERVICE,
                                         UPOWER_PATH,
                                         "org.freedesktop.DBus.Properties",
                                         "PropertiesChanged",
                                         this,
                                         SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    m_lidIsPresent = m_upowerInterface->lidIsPresent();
    setLidPresent(m_lidIsPresent);
    m_lidIsClosed = m_upowerInterface->lidIsClosed();

    // Brightness Controls available
    if (screenBrightnessAvailable) {
        m_screenBrightnessAvailable = true;
        qCDebug(POWERDEVIL) << "current screen brightness value: " << m_cachedScreenBrightness;
    }

    m_kbdBacklight = new OrgFreedesktopUPowerKbdBacklightInterface(UPOWER_SERVICE, "/org/freedesktop/UPower/KbdBacklight", QDBusConnection::systemBus(), this);
    if (m_kbdBacklight->isValid()) {
        // Cache max value
        QDBusPendingReply<int> rep = m_kbdBacklight->GetMaxBrightness();
        rep.waitForFinished();
        if (rep.isValid()) {
            m_kbdMaxBrightness = rep.value();
            m_keyboardBrightnessAvailable = true;
        }
        // TODO Do a proper check if the kbd backlight dbus object exists. But that should work for now ..
        if (m_kbdMaxBrightness) {
            m_cachedKeyboardBrightness = keyboardBrightness();
            qCDebug(POWERDEVIL) << "current keyboard backlight brightness value: " << m_cachedKeyboardBrightness;
            connect(m_kbdBacklight,
                    &OrgFreedesktopUPowerKbdBacklightInterface::BrightnessChangedWithSource,
                    this,
                    &PowerDevilUPowerBackend::onKeyboardBrightnessChanged);
        }
    }

    // backend ready
    setBackendIsReady();
}

void PowerDevilUPowerBackend::onDeviceChanged(const UdevQt::Device &device)
{
    // If we're currently in the process of changing brightness, ignore any such events
    if (m_brightnessAnimationTimer && m_brightnessAnimationTimer->isActive()) {
        return;
    }

    qCDebug(POWERDEVIL) << "Udev device changed" << m_syspath << device.sysfsPath();
    if (device.sysfsPath() != m_syspath) {
        return;
    }

    int maxBrightness = device.sysfsProperty("max_brightness").toInt();
    if (maxBrightness <= 0) {
        return;
    }
    int newBrightness = device.sysfsProperty("brightness").toInt();

    if (newBrightness != m_cachedScreenBrightness) {
        m_cachedScreenBrightness = newBrightness;
        onScreenBrightnessChanged(newBrightness, maxBrightness);
    }
}

int PowerDevilUPowerBackend::screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_screenBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = screenBrightness();
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (!(m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) && currentBrightness != m_cachedScreenBrightness) {
        m_cachedScreenBrightness = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = screenBrightnessMax();
    int newBrightness = calculateNextScreenBrightnessStep(currentBrightness, maxBrightness, type);

    if (newBrightness < 0) {
        return -1;
    }

    setScreenBrightness(newBrightness);
    return newBrightness;
}

int PowerDevilUPowerBackend::keyboardBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!m_keyboardBrightnessAvailable) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int currentBrightness = keyboardBrightness();
    // m_cachedBrightnessMap is not being updated during animation, thus checking the m_cachedBrightnessMap
    // value here doesn't make much sense, use the endValue from brightness() anyway.
    // This prevents brightness key being ignored during the animation.
    if (currentBrightness != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = currentBrightness;
        return currentBrightness;
    }

    int maxBrightness = keyboardBrightnessMax();
    int newBrightness = calculateNextKeyboardBrightnessStep(currentBrightness, maxBrightness, type);

    if (newBrightness < 0) {
        return -1;
    }

    if (type == PowerDevil::BrightnessLogic::BrightnessKeyType::Toggle && newBrightness == 0) {
        setKeyboardBrightnessOff();
    } else {
        setKeyboardBrightness(newBrightness);
    }
    return newBrightness;
}

int PowerDevilUPowerBackend::screenBrightness() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation && m_brightnessAnimation->state() == QPropertyAnimation::Running) {
            result = m_brightnessAnimation->endValue().toInt();
        } else {
            result = m_ddcBrightnessControl->brightness(m_ddcBrightnessControl->displayIds().constFirst());
        }
    } else {
        result = m_cachedScreenBrightness;
    }
    qCDebug(POWERDEVIL) << "Screen brightness value: " << result;
    return result;
}

int PowerDevilUPowerBackend::screenBrightnessMax() const
{
    int result = 0;

    if (m_ddcBrightnessControl->isSupported()) {
        result = m_ddcBrightnessControl->brightnessMax(m_ddcBrightnessControl->displayIds().constFirst());
    } else {
        result = m_brightnessMax;
    }
    qCDebug(POWERDEVIL) << "Screen brightness value max: " << result;

    return result;
}

int PowerDevilUPowerBackend::keyboardBrightnessMax() const
{
    int result = m_kbdMaxBrightness;
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value max: " << result;

    return result;
}

void PowerDevilUPowerBackend::setScreenBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set screen brightness value: " << value;
    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation) {
            m_brightnessAnimation->stop();
            disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->setStartValue(screenBrightness());
            m_brightnessAnimation->setEndValue(value);
            m_brightnessAnimation->setEasingCurve(screenBrightness() < value ? QEasingCurve::OutQuad : QEasingCurve::InQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &PowerDevilUPowerBackend::animationValueChanged);
            m_brightnessAnimation->start();
        } else {
            m_ddcBrightnessControl->setBrightness(m_ddcBrightnessControl->displayIds().constFirst(), value);
        }
    } else {
        KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
        action.setHelperId(HELPER_ID);
        action.addArgument("brightness", value);
        if (screenBrightness() >= PowerDevilSettings::brightnessAnimationThreshold()) {
            action.addArgument("animationDuration", PowerDevilSettings::brightnessAnimationDuration());
        }
        auto *job = action.execute();
        connect(job, &KAuth::ExecuteJob::result, this, [this, job, value] {
            if (job->error()) {
                qCWarning(POWERDEVIL) << "Failed to set screen brightness" << job->errorText();
                return;
            }

            // Immediately announce the new brightness to everyone while we still animate to it
            m_cachedScreenBrightness = value;
            onScreenBrightnessChanged(value, screenBrightnessMax());

            // So we ignore any brightness changes during the animation
            if (!m_brightnessAnimationTimer) {
                m_brightnessAnimationTimer = new QTimer(this);
                m_brightnessAnimationTimer->setSingleShot(true);
            }
            m_brightnessAnimationTimer->start(PowerDevilSettings::brightnessAnimationDuration());
        });
        job->start();
    }
}

bool PowerDevilUPowerBackend::screenBrightnessAvailable() const
{
    return m_screenBrightnessAvailable;
}

void PowerDevilUPowerBackend::setKeyboardBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << value;
    m_kbdBacklight->SetBrightness(value);
    m_keyboardBrightnessBeforeTogglingOff = keyboardBrightness();
}

void PowerDevilUPowerBackend::setKeyboardBrightnessOff()
{
    // save value before toggling so that we can restore it later
    m_keyboardBrightnessBeforeTogglingOff = keyboardBrightness();
    qCDebug(POWERDEVIL) << "set kbd backlight value: " << 0;
    m_kbdBacklight->SetBrightness(0);
}

bool PowerDevilUPowerBackend::keyboardBrightnessAvailable() const
{
    return m_keyboardBrightnessAvailable;
}

void PowerDevilUPowerBackend::slotScreenBrightnessChanged()
{
    if (m_brightnessAnimation && m_brightnessAnimation->state() != QPropertyAnimation::Stopped) {
        return;
    }

    if (m_brightnessAnimationTimer && m_brightnessAnimationTimer->isActive()) {
        return;
    }

    int value = screenBrightness();
    if (value != m_cachedScreenBrightness || m_isLedBrightnessControl) {
        m_cachedScreenBrightness = value;
        onScreenBrightnessChanged(value, screenBrightnessMax());
    }
}

void PowerDevilUPowerBackend::onKeyboardBrightnessChanged(int value, const QString &source)
{
    qCDebug(POWERDEVIL) << "Keyboard brightness changed!!";
    if (value != m_cachedKeyboardBrightness) {
        m_cachedKeyboardBrightness = value;
        BackendInterface::onKeyboardBrightnessChanged(value, keyboardBrightnessMax(), source == QLatin1String("internal"));
        // source: internal = keyboard brightness changed through hardware, eg a firmware-handled hotkey being pressed -> show the OSD
        //         external = keyboard brightness changed through upower -> don't trigger the OSD as we would already have done that where necessary
    }
}

void PowerDevilUPowerBackend::onPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    if (ifaceName != UPOWER_IFACE) {
        return;
    }

    if (m_lidIsPresent) {
        bool lidIsClosed = m_lidIsClosed;
        if (changedProps.contains(QStringLiteral("LidIsClosed"))) {
            lidIsClosed = changedProps[QStringLiteral("LidIsClosed")].toBool();
        } else if (invalidatedProps.contains(QStringLiteral("LidIsClosed"))) {
            lidIsClosed = m_upowerInterface->lidIsClosed();
        }
        if (lidIsClosed != m_lidIsClosed) {
            setButtonPressed(lidIsClosed ? LidClose : LidOpen);
            m_lidIsClosed = lidIsClosed;
        }
    }
}

void PowerDevilUPowerBackend::animationValueChanged(const QVariant &value)
{
    if (m_ddcBrightnessControl->isSupported()) {
        m_ddcBrightnessControl->setBrightness(m_ddcBrightnessControl->displayIds().constFirst(), value.toInt());
    } else {
        qCInfo(POWERDEVIL) << "PowerDevilUPowerBackend::animationValueChanged: brightness control not supported";
    }
}

int PowerDevilUPowerBackend::keyboardBrightness() const
{
    int result = m_kbdBacklight->GetBrightness();
    qCDebug(POWERDEVIL) << "Kbd backlight brightness value: " << result;

    return result;
}

#include "moc_powerdevilupowerbackend.cpp"
