/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "screenbrightnesscontroller.h"

#include <powerdevil_debug.h>

#include <QDBusMessage>
#include <QDebug>
#include <QFileInfo>
#include <QPropertyAnimation>
#include <QTextStream>
#include <QTimer>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kauth_version.h>

#include "brightnessosdwidget.h"
#include "ddcutilbrightness.h"
#include "powerdevil_debug.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

ScreenBrightnessController::ScreenBrightnessController()
    : m_cachedScreenBrightness(0)
    , m_isLedBrightnessControl(false)
{
    connect(this, &ScreenBrightnessController::brightnessSupportQueried, this, &ScreenBrightnessController::initWithBrightness);
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
                    connect(client, &UdevQt::Client::deviceChanged, this, &ScreenBrightnessController::onDeviceChanged);
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

void ScreenBrightnessController::initWithBrightness(bool screenBrightnessAvailable)
{
    if (!screenBrightnessAvailable) {
        qCDebug(POWERDEVIL) << "Brightness Helper have failed. Trying DDC Helper for brightness controls...";
        m_ddcBrightnessControl->detect();
        if (m_ddcBrightnessControl->isSupported()) {
            qCDebug(POWERDEVIL) << "Using DDCutillib";
            m_cachedScreenBrightness = screenBrightness();
            if (m_brightnessAnimationDurationMsec > 0 && screenBrightnessMax() >= m_brightnessAnimationThreshold) {
                m_brightnessAnimation = new QPropertyAnimation(this);
                m_brightnessAnimation->setTargetObject(this);
                m_brightnessAnimation->setDuration(m_brightnessAnimationDurationMsec);
                connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
                connect(m_brightnessAnimation, &QPropertyAnimation::finished, this, &ScreenBrightnessController::slotScreenBrightnessChanged);
            }
            screenBrightnessAvailable = true;
        }
    }
    disconnect(this, &ScreenBrightnessController::brightnessSupportQueried, this, &ScreenBrightnessController::initWithBrightness);
    // Brightness Controls available
    if (screenBrightnessAvailable) {
        m_screenBrightnessAvailable = true;
        qCDebug(POWERDEVIL) << "current screen brightness value: " << m_cachedScreenBrightness;
    }
    Q_EMIT detectionFinished();
}

int ScreenBrightnessController::screenBrightnessSteps()
{
    m_screenBrightnessLogic.setValueMax(screenBrightnessMax());
    return m_screenBrightnessLogic.steps();
}

int ScreenBrightnessController::calculateNextScreenBrightnessStep(int value, int valueMax, PowerDevil::BrightnessLogic::BrightnessKeyType keyType)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    return m_screenBrightnessLogic.action(keyType);
}

void ScreenBrightnessController::onDeviceChanged(const UdevQt::Device &device)
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

int ScreenBrightnessController::screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
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

int ScreenBrightnessController::screenBrightness() const
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

int ScreenBrightnessController::screenBrightnessMax() const
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

void ScreenBrightnessController::setScreenBrightness(int value)
{
    qCDebug(POWERDEVIL) << "set screen brightness value: " << value;
    if (m_ddcBrightnessControl->isSupported()) {
        if (m_brightnessAnimation) {
            m_brightnessAnimation->stop();
            disconnect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
            m_brightnessAnimation->setStartValue(screenBrightness());
            m_brightnessAnimation->setEndValue(value);
            m_brightnessAnimation->setEasingCurve(screenBrightness() < value ? QEasingCurve::OutQuad : QEasingCurve::InQuad);
            connect(m_brightnessAnimation, &QPropertyAnimation::valueChanged, this, &ScreenBrightnessController::animationValueChanged);
            m_brightnessAnimation->start();
        } else {
            for (const QString &displayId : m_ddcBrightnessControl->displayIds()) {
                m_ddcBrightnessControl->setBrightness(displayId, value);
            }
        }
    } else {
        KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
        action.setHelperId(HELPER_ID);
        action.addArgument("brightness", value);
        if (screenBrightness() >= m_brightnessAnimationThreshold) {
            action.addArgument("animationDuration", m_brightnessAnimationDurationMsec);
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
            m_brightnessAnimationTimer->start(m_brightnessAnimationDurationMsec);
        });
        job->start();
    }
}

bool ScreenBrightnessController::screenBrightnessAvailable() const
{
    return m_screenBrightnessAvailable;
}

void ScreenBrightnessController::slotScreenBrightnessChanged()
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

void ScreenBrightnessController::animationValueChanged(const QVariant &value)
{
    if (m_ddcBrightnessControl->isSupported()) {
        for (const QString &displayId : m_ddcBrightnessControl->displayIds()) {
            m_ddcBrightnessControl->setBrightness(displayId, value.toInt());
        }
    } else {
        qCInfo(POWERDEVIL) << "ScreenBrightnessController::animationValueChanged: brightness control not supported";
    }
}

void ScreenBrightnessController::onScreenBrightnessChanged(int value, int valueMax)
{
    m_screenBrightnessLogic.setValueMax(valueMax);
    m_screenBrightnessLogic.setValue(value);

    Q_EMIT screenBrightnessChanged(m_screenBrightnessLogic.info());
}

#include "moc_screenbrightnesscontroller.cpp"
