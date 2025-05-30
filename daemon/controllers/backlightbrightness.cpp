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

#include <memory> // std::shared_ptr

#include "udevqt.h"

using namespace Qt::StringLiterals;

inline constexpr QLatin1StringView HELPER_ID("org.kde.powerdevil.backlighthelper");

BacklightDetector::BacklightDetector(QObject *parent)
    : DisplayBrightnessDetector(parent)
{
}

void BacklightDetector::detect()
{
    if (m_display) {
        disconnect(m_display.get(), nullptr, nullptr, nullptr);
    }
    std::shared_ptr<BacklightBrightness> deleteOld(m_display.release());

    KAuth::Action brightnessAction(u"org.kde.powerdevil.backlighthelper.brightness"_s);
    brightnessAction.setHelperId(HELPER_ID);
    KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
    connect(brightnessJob, &KJob::result, this, [this, brightnessJob, deleteOld = std::move(deleteOld)] {
        if (brightnessJob->error()) {
            qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
            qCDebug(POWERDEVIL) << brightnessJob->errorText();
            Q_EMIT detectionFinished(false);
            return;
        }
        int cachedBrightness = brightnessJob->data()[u"brightness"_s].toInt();

        KAuth::Action brightnessMaxAction(u"org.kde.powerdevil.backlighthelper.brightnessmax"_s);
        brightnessMaxAction.setHelperId(HELPER_ID);
        KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
        connect(brightnessMaxJob, &KJob::result, this, [this, brightnessMaxJob, cachedBrightness, deleteOld = std::move(deleteOld)] {
            if (brightnessMaxJob->error()) {
                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
                qCDebug(POWERDEVIL) << brightnessMaxJob->errorText();
                Q_EMIT detectionFinished(false);
                return;
            }
            int maxBrightness = brightnessMaxJob->data()[u"brightnessmax"_s].toInt();

#ifdef Q_OS_FREEBSD
            // FreeBSD doesn't have the sysfs interface that the bits below expect;
            // the sysfs calls always fail, leading to detectionFinished(false) emission.
            // Skip that command and carry on with the information that we do have.
            if (maxBrightness > 0) {
                m_display.reset(new BacklightBrightness(cachedBrightness, maxBrightness, QString()));
            }
            Q_EMIT detectionFinished(m_display != nullptr);
#else
            KAuth::Action syspathAction(u"org.kde.powerdevil.backlighthelper.syspath"_s);
            syspathAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob* syspathJob = syspathAction.execute();
            connect(syspathJob, &KJob::result, this, [this, syspathJob, cachedBrightness, maxBrightness, deleteOld = std::move(deleteOld)] {
                if (syspathJob->error()) {
                    qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.syspath failed";
                    qCDebug(POWERDEVIL) << syspathJob->errorText();
                    Q_EMIT detectionFinished(false);
                    return;
                }
                if (maxBrightness > 0) {
                    QString syspath = syspathJob->data()[u"syspath"_s].toString();
                    syspath = QFileInfo(syspath).symLinkTarget();
                    m_display.reset(new BacklightBrightness(cachedBrightness, maxBrightness, syspath));
                }
                Q_EMIT detectionFinished(m_display != nullptr);
            });
            syspathJob->start();
#endif
        });
        brightnessMaxJob->start();
    });
    brightnessJob->start();
}

QList<DisplayBrightness *> BacklightDetector::displays() const
{
    return m_display ? QList<DisplayBrightness *>(1, m_display.get()) : QList<DisplayBrightness *>();
}

BacklightBrightness::BacklightBrightness(int observedBrightness, int maxBrightness, QString syspath, QObject *parent)
    : DisplayBrightness(parent)
    , m_syspath(syspath)
    , m_observedBrightness(observedBrightness)
    , m_requestedBrightness(observedBrightness)
    , m_executedBrightness(observedBrightness)
    , m_maxBrightness(maxBrightness)
{
#ifndef Q_OS_FREEBSD
    // Kernel doesn't send uevent for leds-class devices, or at least that's what commit 26a48f9db
    // claimed (although the monitored subsystem was already hardcoded to "backlight" then).
    // Keep track of backlight devices for changes not initiated by this class.
    if (!m_syspath.contains(QLatin1String("/leds/"))) {
        UdevQt::Client *client = new UdevQt::Client(QStringList{u"backlight"_s}, this);
        connect(client, &UdevQt::Client::deviceChanged, this, &BacklightBrightness::onDeviceChanged);
    }
#endif
}

bool BacklightBrightness::isSupported() const
{
    return m_maxBrightness > 0;
}

void BacklightBrightness::onDeviceChanged(const UdevQt::Device &device)
{
    if (device.sysfsPath() != m_syspath) {
        return;
    }
    int maxBrightness = device.sysfsProperty(u"max_brightness"_s).toInt();
    if (maxBrightness <= 0) {
        return;
    }
    int newBrightness = device.sysfsProperty(u"brightness"_s).toInt();
    if (m_observedBrightness == newBrightness) {
        return;
    }
    int lastObservedBrightness = m_observedBrightness;
    m_observedBrightness = newBrightness;

    qCDebug(POWERDEVIL) << "[BacklightBrightness]: Udev device changed brightness to:" << m_observedBrightness << "for" << m_syspath;

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

    qCDebug(POWERDEVIL) << "[BacklightBrightness]: External brightness change observed:" << m_observedBrightness << "/" << maxBrightness;
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
    return m_maxBrightness;
}

int BacklightBrightness::brightness() const
{
    return m_requestedBrightness;
}

void BacklightBrightness::setBrightness(int newBrightness, bool allowAnimations)
{
    if (!isSupported()) {
        qCWarning(POWERDEVIL) << "[BacklightBrightness]: Not supported, setBrightness() should not be called";
        return;
    }
    if (newBrightness < 0 || newBrightness > maxBrightness()) {
        qCWarning(POWERDEVIL) << "[BacklightBrightness]: Invalid brightness requested:" << newBrightness << "- ignoring | valid range: 0 to" << maxBrightness();
        return;
    }
    m_requestedBrightness = newBrightness;

    if (m_isWaitingForKAuthJob) {
        // There's already a KAuth setbrightness job running. Don't start a second one until
        // this one has replied. (Note that when the setbrightness job returns, the animation
        // has been started but will probably still run asynchronously for a while.)
        // Remember m_requestedBrightness for later and handle it in the result handler lambda.
        return;
    }
    m_isWaitingForKAuthJob = true;

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
    const bool willAnimate = brightnessDiff >= m_brightnessAnimationThreshold && allowAnimations;

    KAuth::Action action(u"org.kde.powerdevil.backlighthelper.setbrightness"_s);
    action.setHelperId(HELPER_ID);
    action.addArgument(u"brightness"_s, newBrightness);
    action.addArgument(u"animationDuration"_s, willAnimate ? m_brightnessAnimationDurationMsec : 0);
    auto *job = action.execute();

    connect(job, &KAuth::ExecuteJob::result, this, [this, job, willAnimate] {
        Q_ASSERT(m_isWaitingForKAuthJob);
        m_isWaitingForKAuthJob = false;

        if (job->error()) {
            qCWarning(POWERDEVIL) << "[BacklightBrightness]: Failed to set screen brightness" << job->errorText();
            Q_EMIT externalBrightnessChangeObserved(this, m_observedBrightness);
            return;
        }

        if (m_requestedBrightness != m_executedBrightness) {
            // We had another setBrightness() request come in in the meantime:
            // apply it now that m_isWaitingForKAuthJob is not blocking the call
            setBrightness(m_requestedBrightness, willAnimate);
        }
    });
    job->start();
}

bool BacklightBrightness::isInternal() const
{
    return true;
}

#include "moc_backlightbrightness.cpp"
