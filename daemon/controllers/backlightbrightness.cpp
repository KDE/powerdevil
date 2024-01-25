/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "backlightbrightness.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KAuth/ExecuteJob>
#include <KAuth/HelperSupport>

#include <KLocalizedString>

#include "udevqt.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

BacklightBrightness::BacklightBrightness(QObject *parent)
    : QObject(parent)
{
}

void BacklightBrightness::detect()
{
    KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
    brightnessAction.setHelperId(HELPER_ID);
    KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
    connect(brightnessJob, &KJob::result, this, [this, brightnessJob] {
        if (brightnessJob->error()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
            qCDebug(POWERDEVIL) << brightnessJob->errorText();
            Q_EMIT detectionFinished(isSupported());
            return;
        }
        m_cachedBrightness = brightnessJob->data()["brightness"].toFloat();

        KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
        brightnessMaxAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
        connect(brightnessMaxJob, &KJob::result, this, [this, brightnessMaxJob] {
            if (brightnessMaxJob->error()) {
                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
                qCDebug(POWERDEVIL) << brightnessMaxJob->errorText();
            } else {
                m_maxBrightness = brightnessMaxJob->data()["brightnessmax"].toInt();
            }

#ifdef Q_OS_FREEBSD
            // FreeBSD doesn't have the sysfs interface that the bits below expect;
            // the sysfs calls always fail, leading to detectionFinished(false) emission.
            // Skip that command and carry on with the information that we do have.
            Q_EMIT detectionFinished(isSupported());
#else
            KAuth::Action syspathAction("org.kde.powerdevil.backlighthelper.syspath");
            syspathAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob* syspathJob = syspathAction.execute();
            connect(syspathJob, &KJob::result, this, [this, syspathJob] {
                if (syspathJob->error()) {
                    qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.syspath failed";
                    qCDebug(POWERDEVIL) << syspathJob->errorText();
                    m_maxBrightness = 0; // i.e. isSupported() == false
                    Q_EMIT detectionFinished(isSupported());
                    return;
                }
                m_syspath = syspathJob->data()["syspath"].toString();
                m_syspath = QFileInfo(m_syspath).symLinkTarget();

                // Kernel doesn't send uevent for leds-class devices, or at least that's what
                // commit 26a48f9db claimed (although the monitored subsystem was already hardcoded
                // to "backlight" then). That's okay because we emit brightnessChanged() at least
                // once when setBrightness() starts successfully.
                if (!m_syspath.contains(QLatin1String("/leds/"))) {
                    UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
                    connect(client, &UdevQt::Client::deviceChanged, this, &BacklightBrightness::onDeviceChanged);
                }

                Q_EMIT detectionFinished(isSupported());
            });
            syspathJob->start();
#endif
        });
        brightnessMaxJob->start();
    });
    brightnessJob->start();
}

QString BacklightBrightness::label() const
{
    return i18n("Built-in Screen");
}

bool BacklightBrightness::isSupported() const
{
    return m_maxBrightness > 0;
}

void BacklightBrightness::onDeviceChanged(const UdevQt::Device &device)
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

    if (newBrightness != m_cachedBrightness) {
        m_cachedBrightness = newBrightness;
        Q_EMIT brightnessChanged(newBrightness, maxBrightness);
    }
}

int BacklightBrightness::maxBrightness() const
{
    return m_maxBrightness;
}

int BacklightBrightness::brightness() const
{
    return m_cachedBrightness;
}

void BacklightBrightness::setBrightness(int newBrightness, int animationDurationMsec)
{
    if (!isSupported()) {
        qCWarning(POWERDEVIL) << "backlight not supported, setBrightness() should not be called";
        return;
    }

    KAuth::Action action("org.kde.powerdevil.backlighthelper.setbrightness");
    action.setHelperId(HELPER_ID);
    action.addArgument("brightness", newBrightness);
    if (brightness() >= m_brightnessAnimationThreshold) {
        action.addArgument("animationDuration", animationDurationMsec);
    }
    auto *job = action.execute();

    connect(job, &KAuth::ExecuteJob::result, this, [this, job, newBrightness, animationDurationMsec] {
        if (job->error()) {
            qCWarning(POWERDEVIL) << "Failed to set screen brightness" << job->errorText();
            return;
        }

        // So we ignore any brightness changes during the animation
        if (!m_brightnessAnimationTimer) {
            m_brightnessAnimationTimer = new QTimer(this);
            m_brightnessAnimationTimer->setSingleShot(true);
        }
        m_brightnessAnimationTimer->start(animationDurationMsec);

        // Immediately announce the new brightness to everyone while we still animate to it
        m_cachedBrightness = newBrightness;
        Q_EMIT brightnessChanged(newBrightness, maxBrightness());
    });
    job->start();
}

QString BacklightBrightness::generateDisplayId() const
{
    return m_syspath.split(QLatin1Char('/')).last();
}

#include "moc_backlightbrightness.cpp"
