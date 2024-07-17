/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "screenbrightnesscontroller.h"

#include "backlightbrightness.h"
#include "ddcutildetector.h"
#include "displaybrightness.h"
#include "externalbrightnesscontrol.h"
#include "kwinbrightness.h"

#include <brightnessosdwidget.h>
#include <powerdevil_debug.h>

#include <KScreen/ConfigMonitor>
#include <KScreen/EDID>
#include <KScreen/GetConfigOperation>
#include <KScreen/Output>

#include <QDebug>
#include <QPropertyAnimation>
#include <ranges>

#include <algorithm> // std::ranges::find_if

ScreenBrightnessController::ScreenBrightnessController()
    : QObject()
    , m_detectors({
          {
              .detector = new KWinDisplayDetector(this),
              .debugName = "kwin brightness control",
              .displayIdPrefix = "kwin:",
          },
          {
              .detector = new BacklightDetector(this),
              .debugName = "internal display backlight",
              .displayIdPrefix = "backlight:",
          },
          {
              .detector = new DDCutilDetector(this),
              .debugName = "libddcutil",
              .displayIdPrefix = "ddc:",
          },
      })
    , m_externalBrightnessController(std::make_unique<ExternalBrightnessController>())
{
    connect(m_externalBrightnessController.get(), &ExternalBrightnessController::activeChanged, this, &ScreenBrightnessController::onDetectorDisplaysChanged);

    const auto op = new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoOptions, this);
    connect(op, &KScreen::GetConfigOperation::finished, this, [this](KScreen::ConfigOperation *configOp) {
        if (configOp->hasError()) {
            return;
        }
        m_kscreenConfig = static_cast<KScreen::GetConfigOperation *>(configOp)->config();
        KScreen::ConfigMonitor::instance()->addConfig(m_kscreenConfig);
    });
}

ScreenBrightnessController::~ScreenBrightnessController()
{
}

void ScreenBrightnessController::detectDisplays()
{
    qCDebug(POWERDEVIL) << "Trying to detect displays for brightness control...";
    m_finishedDetectingCount = 0;

    for (const DetectorInfo &detectorInfo : m_detectors) {
        DisplayBrightnessDetector *detector = detectorInfo.detector;
        disconnect(detector, nullptr, this, nullptr);

        connect(detector, &DisplayBrightnessDetector::detectionFinished, this, [this, detector]() {
            disconnect(detector, &DisplayBrightnessDetector::detectionFinished, this, nullptr);

            if (++m_finishedDetectingCount; m_finishedDetectingCount == m_detectors.size()) {
                onDetectorDisplaysChanged();
                Q_EMIT detectionFinished();
            }
            connect(detector, &DisplayBrightnessDetector::displaysChanged, this, &ScreenBrightnessController::onDetectorDisplaysChanged);
        });
        detector->detect();
    }
}

bool ScreenBrightnessController::isSupported() const
{
    return !m_sortedDisplayIds.isEmpty();
}

QStringList ScreenBrightnessController::displayIds() const
{
    return m_sortedDisplayIds;
}

void ScreenBrightnessController::onDisplayDestroyed(QObject *obj)
{
    for (auto &[id, info] : m_displaysById) {
        if (info.display == obj) {
            // we'll do the proper removal in onDetectorDisplaysChanged() which should come
            // right afterwards, just don't call it anymore including through QObject::disconnect()
            info.zombie = true;
        }
    }
}

void ScreenBrightnessController::onDetectorDisplaysChanged()
{
    m_sortedDisplayIds.clear();
    QStringList legacyDisplayIds;

    std::unordered_map<QString, DisplayInfo> newDisplayById;
    QList<DisplayBrightness *> newForExternalControl;

    // for backwards compatibility with legacy API clients, set the same brightness to all displays
    // of the first detector - e.g. to only the backlight display, or to all external DDC monitors
    DisplayBrightnessDetector *firstSupportedDetector = nullptr;

    // add new displays
    for (const DetectorInfo &detectorInfo : m_detectors) {
        const bool shouldUseExternalControl = m_externalBrightnessController->isActive() && !dynamic_cast<KWinDisplayDetector *>(detectorInfo.detector);
        QList<DisplayBrightness *> detectorDisplays = detectorInfo.detector->displays();

        if (!detectorDisplays.isEmpty() && !shouldUseExternalControl) {
            if (firstSupportedDetector == nullptr) {
                firstSupportedDetector = detectorInfo.detector;
            }
            qCDebug(POWERDEVIL) << "Using" << detectorInfo.debugName << "for brightness controls.";
        }
        for (DisplayBrightness *display : std::as_const(detectorDisplays)) {
            const QString displayId = QString::fromLocal8Bit(detectorInfo.displayIdPrefix) + display->id();
            if (shouldUseExternalControl) {
                newForExternalControl.push_back(display);
            } else {
                auto &info = newDisplayById[displayId];
                info = DisplayInfo{
                    .display = display,
                    .detector = detectorInfo.detector,
                };
                info.brightnessLogic.setValueRange(display->knownSafeMinBrightness(), display->maxBrightness());
                info.brightnessLogic.setValue(display->brightness());
                m_sortedDisplayIds.push_back(displayId);
                if (detectorInfo.detector == firstSupportedDetector) {
                    legacyDisplayIds.append(displayId);
                }
            }
        }
    }

    QStringList removedDisplayIds;
    for (const auto &[displayId, info] : m_displaysById) {
        const auto it = newDisplayById.find(displayId);
        const bool removed = it == newDisplayById.end();
        const bool replaced = !removed && it->second.display != info.display;
        if ((removed || replaced) && !info.zombie) {
            disconnect(info.display, nullptr, this, nullptr);
        }
        if (removed) {
            removedDisplayIds.push_back(displayId);
        }
    }

    QStringList addedDisplayIds;
    QStringList brightnessChangedDisplayIds;
    for (auto &[displayId, info] : newDisplayById) {
        const auto it = m_displaysById.find(displayId);
        const bool added = it == m_displaysById.end();
        const bool replaced = !added && info.display != it->second.display;
        const bool valueChanged = replaced
            && (it->second.brightnessLogic.info().value != info.display->brightness()
                || it->second.brightnessLogic.info().valueMax != info.display->maxBrightness());
        if (added || replaced) {
            connect(info.display, &QObject::destroyed, this, &ScreenBrightnessController::onDisplayDestroyed);
            connect(info.display, &DisplayBrightness::externalBrightnessChangeObserved, this, &ScreenBrightnessController::onExternalBrightnessChangeObserved);
        } else {
            // migrate any local state from the old element to its replacement
            info.trackingError = it->second.trackingError;
        }
        if (valueChanged) {
            brightnessChangedDisplayIds.push_back(displayId);
        }
        if (added) {
            addedDisplayIds.push_back(displayId);
        }
    }

    m_displaysById = std::move(newDisplayById);
    m_externalBrightnessController->setDisplays(newForExternalControl);

    for (const QString &removed : removedDisplayIds) {
        Q_EMIT displayRemoved(removed);
    }
    for (const QString &added : addedDisplayIds) {
        Q_EMIT displayAdded(added);
    }
    for (const QString &changed : brightnessChangedDisplayIds) {
        const auto &[id, info] = *m_displaysById.find(changed);
        qCDebug(POWERDEVIL) << "Screen brightness of display" << changed << "after detection/reconfiguration:" << info.brightnessLogic.info().value;
        Q_EMIT brightnessChanged(changed, info.brightnessLogic.info(), QString(), QString(), SuppressIndicator);
    }

    const QString previousFirstDisplayId = m_legacyDisplayIds.value(0, QString());

    if (m_legacyDisplayIds != legacyDisplayIds) {
        m_legacyDisplayIds = legacyDisplayIds;
        Q_EMIT legacyDisplayIdsChanged(m_legacyDisplayIds);
    }

    if (!isSupported()) {
        qCDebug(POWERDEVIL) << "No suitable displays detected. Brightness controls are unsupported in this configuration.";
        return;
    }

    // legacy API needs to emit a brightness change signal for the first display, regardless of
    // whether the new first display was newly added or an existing display moved up to index 0
    const QString &newFirstDisplayId = m_legacyDisplayIds.first();
    if (newFirstDisplayId != previousFirstDisplayId || brightnessChangedDisplayIds.contains(newFirstDisplayId)) {
        const auto &[id, info] = *m_displaysById.find(newFirstDisplayId);
        qCDebug(POWERDEVIL) << "Screen brightness of first display after detection/reconfiguration:" << info.brightnessLogic.info().value;
        Q_EMIT legacyBrightnessInfoChanged(info.brightnessLogic.info(), SuppressIndicator);
    }
}

QString ScreenBrightnessController::label(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
        QString result = it->second.display->label();
        qCDebug(POWERDEVIL) << "Screen label of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen label failed: no display with id" << displayId;
    return QString();
}

bool ScreenBrightnessController::isInternal(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
        bool result = it->second.display->isInternal();
        qCDebug(POWERDEVIL) << "Screen " << displayId << (result ? "is internal" : "is not internal");
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen is internal failed: no display with id" << displayId;
    return false;
}

int ScreenBrightnessController::knownSafeMinBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
        int result = it->second.display->knownSafeMinBrightness();
        qCDebug(POWERDEVIL) << "Screen knownSafeMinBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen knownSafeMinBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::minBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        int result = it->second.brightnessLogic.info().valueMin;
        qCDebug(POWERDEVIL) << "Screen minBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen minBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::maxBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        int result = it->second.brightnessLogic.info().valueMax;
        qCDebug(POWERDEVIL) << "Screen maxBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen maxBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::brightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        int result = it->second.brightnessLogic.info().value;
        qCDebug(POWERDEVIL) << "Screen brightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen brightness failed: no display with id" << displayId;
    return 0;
}

void ScreenBrightnessController::setBrightness(const QString &displayId,
                                               int value,
                                               const QString &sourceClientName,
                                               const QString &sourceClientContext,
                                               IndicatorHint hint)
{
    if (auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
        auto &[id, info] = *it;
        const PowerDevil::BrightnessLogic::BrightnessInfo bi = info.brightnessLogic.info();
        const int boundedValue = qBound(bi.valueMin, value, bi.valueMax);

        qCDebug(POWERDEVIL) << "Set screen brightness of" << displayId << "to" << boundedValue << "/" << bi.valueMax;
        if (value != boundedValue) {
            qCDebug(POWERDEVIL) << "- clamped from" << value;
        }

        // notify only when the internally tracked brightness value is actually different
        if (bi.value != boundedValue) {
            info.brightnessLogic.setValue(boundedValue);
            info.trackingError = 0.0;
            Q_EMIT brightnessChanged(displayId, info.brightnessLogic.info(), sourceClientName, sourceClientContext, hint);

            // legacy API without displayId parameter: notify only if the first supported display changed
            if (displayId == m_legacyDisplayIds.first()) {
                Q_EMIT legacyBrightnessInfoChanged(info.brightnessLogic.info(), hint);
            }
        }

        // but always call setBrightness() on the display, in case we're unaware of an external change
        info.display->setBrightness(boundedValue);
    } else {
        qCWarning(POWERDEVIL) << "Set screen brightness failed: no display with id" << displayId;
    }
}

void ScreenBrightnessController::adjustBrightnessRatio(const QString &displayId,
                                                       double delta,
                                                       const QString &sourceClientName,
                                                       const QString &sourceClientContext,
                                                       IndicatorHint hint)
{
    if (auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
        auto &[id, info] = *it;
        double oldRatio = info.brightnessLogic.valueAsRatio();
        double targetRatio = oldRatio + delta + info.trackingError;

        setBrightness(displayId, info.brightnessLogic.valueFromRatio(targetRatio), sourceClientName, sourceClientContext, hint);
        info.trackingError = targetRatio - info.brightnessLogic.valueAsRatio();
    } else {
        qCWarning(POWERDEVIL) << "Adjust screen brightness ratio failed: no display with id" << displayId;
    }
}

void ScreenBrightnessController::adjustBrightnessRatio(double delta, const QString &sourceClientName, const QString &sourceClientContext, IndicatorHint hint)
{
    if (m_sortedDisplayIds.isEmpty()) {
        qCWarning(POWERDEVIL) << "Adjust screen brightness ratio failed: no displays available to adjust";
        return;
    }

    // if we're going to adjust brightness and accumulate tracking errors, let's make sure at least
    // one display will actually change its brightness as a result
    bool any = std::ranges::any_of(std::as_const(m_sortedDisplayIds), [this, delta](const QString &displayId) {
        if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
            // return true if the display still has room to go in the direction of the delta
            const PowerDevil::BrightnessLogic::BrightnessInfo bi = it->second.brightnessLogic.info();
            return (bi.value < bi.valueMax && delta > 0.0) || (bi.value > bi.valueMin && delta < 0.0);
        }
        return false;
    });
    if (!any) {
        return;
    }

    for (const QString &displayId : std::as_const(m_sortedDisplayIds)) {
        adjustBrightnessRatio(displayId, delta, sourceClientName, sourceClientContext, hint);
    }
}

void ScreenBrightnessController::adjustBrightnessStep(PowerDevil::BrightnessLogic::StepAdjustmentAction adjustment,
                                                      const QString &sourceClientName,
                                                      const QString &sourceClientContext,
                                                      IndicatorHint hint)
{
    if (m_sortedDisplayIds.isEmpty()) {
        qCWarning(POWERDEVIL) << "Adjust screen brightness step failed: no displays available to adjust";
        return;
    }

    double referenceDisplayDelta = 0.0;

    for (const QString &displayId : std::as_const(m_sortedDisplayIds)) {
        if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie) {
            const auto &[id, info] = *it;
            double oldRatio = info.brightnessLogic.valueAsRatio();

            int referenceDisplayBrightness = info.brightnessLogic.adjusted(adjustment);
            if (referenceDisplayBrightness < 0) {
                return;
            }
            if (referenceDisplayBrightness != info.brightnessLogic.info().value) {
                referenceDisplayDelta = info.brightnessLogic.ratio(referenceDisplayBrightness) - oldRatio;
                break;
            }
        }
    }

    if (referenceDisplayDelta == 0.0) {
        return;
    }
    for (const QString &displayId : std::as_const(m_sortedDisplayIds)) {
        adjustBrightnessRatio(displayId, referenceDisplayDelta, sourceClientName, sourceClientContext, hint);
    }
}

int ScreenBrightnessController::brightnessSteps(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        return it->second.brightnessLogic.steps();
    }
    qCWarning(POWERDEVIL) << "Query screen brightnessSteps failed: no display with id" << displayId;
    return 1;
}

KScreen::OutputPtr ScreenBrightnessController::tryMatchKScreenOutput(const QString &displayId) const
{
    if (const auto it = m_displaysById.find(displayId); it != m_displaysById.end() && !it->second.zombie && m_kscreenConfig) {
        const DisplayBrightness *display = it->second.display;
        for (const KScreen::OutputPtr &output : m_kscreenConfig->outputs()) {
            if (display->id() == output->name()) { // for KWinDisplayDetector, primarily
                return output;
            }
            if (output->type() == KScreen::Output::Panel && display->isInternal()) {
                return output;
            }
            const std::optional<QByteArray> displayEdid = display->edidData();
            if (displayEdid && output->edid() && output->edid()->rawData().startsWith(*displayEdid)) {
                return output;
            }
        }
    } else if (!m_kscreenConfig) {
        qCWarning(POWERDEVIL) << "Match KScreen::Output failed: config not initialized";
        return KScreen::OutputPtr{};
    } else {
        qCWarning(POWERDEVIL) << "Match KScreen::Output failed: no display with id" << displayId;
        return KScreen::OutputPtr{};
    }
    // no match found (not an error)
    return KScreen::OutputPtr{};
}

void ScreenBrightnessController::onExternalBrightnessChangeObserved(DisplayBrightness *display, int value)
{
    const auto it = std::ranges::find_if(m_displaysById, [display](const std::pair<QString, DisplayInfo> &pair) {
        return pair.second.display == display;
    });
    if (it == m_displaysById.end()) {
        qCWarning(POWERDEVIL) << "External brightness change of untracked display" << display->id() << "to" << value << "/" << display->maxBrightness();
        return;
    }
    auto &[displayId, info] = *it;
    if (value == info.brightnessLogic.info().value) {
        qCDebug(POWERDEVIL) << "External brightness change of display" << displayId << "ignored - same as previous value";
        return;
    }
    qCDebug(POWERDEVIL) << "External brightness change of display" << displayId << "to" << value << "/" << info.brightnessLogic.info().valueMax;

    info.brightnessLogic.setValue(value);
    info.trackingError = 0.0;

    Q_EMIT brightnessChanged(displayId, info.brightnessLogic.info(), QString(), QString(), SuppressIndicator);

    // legacy API without displayId parameter: notify only if the first supported display changed
    if (displayId == m_legacyDisplayIds.first()) {
        Q_EMIT legacyBrightnessInfoChanged(info.brightnessLogic.info(), SuppressIndicator);
    }
}

//
// legacy API without displayId parameter

QStringList ScreenBrightnessController::legacyDisplayIds() const
{
    return m_legacyDisplayIds;
}

int ScreenBrightnessController::knownSafeMinBrightness() const
{
    return knownSafeMinBrightness(m_legacyDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::minBrightness() const
{
    return minBrightness(m_legacyDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::maxBrightness() const
{
    return maxBrightness(m_legacyDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::brightness() const
{
    return brightness(m_legacyDisplayIds.value(0, QString()));
}

void ScreenBrightnessController::setBrightness(int value, IndicatorHint hint)
{
    if (m_legacyDisplayIds.isEmpty()) {
        qCWarning(POWERDEVIL) << "Set screen brightness failed: no supported display available";
    }
    for (const QString &displayId : std::as_const(m_legacyDisplayIds)) {
        setBrightness(displayId, value, QString(), QString(), hint);
    }
}

int ScreenBrightnessController::brightnessSteps() const
{
    return brightnessSteps(m_legacyDisplayIds.value(0, QString()));
}

#include "moc_screenbrightnesscontroller.cpp"
