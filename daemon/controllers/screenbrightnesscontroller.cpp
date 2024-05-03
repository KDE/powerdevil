/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "screenbrightnesscontroller.h"

#include "backlightbrightness.h"
#include "ddcutildetector.h"
#include "displaybrightness.h"

#include <brightnessosdwidget.h>
#include <powerdevil_debug.h>

#include <QDebug>
#include <QPropertyAnimation>

#include <algorithm> // std::find_if

ScreenBrightnessController::ScreenBrightnessController()
    : QObject()
    , m_detectors({
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
{
}

void ScreenBrightnessController::detectDisplays()
{
    disconnect(nullptr, &DisplayBrightness::externalBrightnessChangeObserved, this, &ScreenBrightnessController::onExternalBrightnessChangeObserved);
    disconnect(nullptr, &DisplayBrightnessDetector::displaysChanged, this, &ScreenBrightnessController::onDetectorDisplaysChanged);

    qCDebug(POWERDEVIL) << "Trying to detect displays for brightness control...";
    m_finishedDetectingCount = 0;
    m_sortedDisplayIds.clear();
    m_displaysById.clear();

    for (const DetectorInfo &detectorInfo : m_detectors) {
        DisplayBrightnessDetector *detector = detectorInfo.detector;

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

void ScreenBrightnessController::onDetectorDisplaysChanged()
{
    const QString previousFirstDisplayId = m_sortedDisplayIds.value(0, QString());

    // don't clear m_displaysById, we'll diff its items for proper signal connection management
    // and other state preservation
    m_sortedDisplayIds.clear();
    QStringList addedDisplayIds;
    QStringList brightnessChangedDisplayIds;
    QStringList restoreBrightnessDisplayIds;

    // To ensure backward compatibility with legacy API clients as well as minimize unintended
    // dimming that we can't track well across reboots/hotplugging, we'll default to only managing
    // brightness for all displays of the first non-empty detector.
    // i.e. either only the backlight display, or to all external DDC monitors.
    // This is the same set of displays as is affected by legacy setBrightness() without displayId,
    // except here we only initialize isManaged for added displays (no changes to known displays).
    DisplayBrightnessDetector *firstSupportedDetector = nullptr;

    // add new displays
    for (const DetectorInfo &detectorInfo : m_detectors) {
        QList<DisplayBrightness *> detectorDisplays = detectorInfo.detector->displays();

        if (!detectorDisplays.isEmpty()) {
            qCDebug(POWERDEVIL) << "Using" << detectorInfo.debugName << "for brightness controls.";
        }
        for (DisplayBrightness *display : detectorDisplays) {
            const QString displayId = detectorInfo.displayIdPrefix + display->id();
            m_sortedDisplayIds.append(displayId);

            if (firstSupportedDetector == nullptr) {
                firstSupportedDetector = detectorInfo.detector;
            }

            if (auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
                if (it->display != display) { // same displayId, different object: update all values
                    disconnect(it->display,
                               &DisplayBrightness::externalBrightnessChangeObserved,
                               this,
                               &ScreenBrightnessController::onExternalBrightnessChangeObserved);
                    connect(display,
                            &DisplayBrightness::externalBrightnessChangeObserved,
                            this,
                            &ScreenBrightnessController::onExternalBrightnessChangeObserved);

                    it->display = display;

                    const int value = display->brightness();
                    const int maxValue = display->maxBrightness();
                    const PowerDevil::BrightnessLogic::BrightnessInfo previous = it->brightnessLogic.info();

                    it->brightnessLogic.setValueRange(display->knownSafeMinBrightness(), maxValue);
                    it->brightnessLogic.setValue(value);

                    if (maxValue != previous.valueMax) {
                        brightnessChangedDisplayIds.append(displayId);
                    }
                    if (it->isManaged && maxValue == previous.valueMax && value != previous.value) {
                        restoreBrightnessDisplayIds.append(displayId);
                    }
                }
            } else {
                connect(display, &DisplayBrightness::externalBrightnessChangeObserved, this, &ScreenBrightnessController::onExternalBrightnessChangeObserved);

                auto &info = m_displaysById[displayId] = DisplayInfo{
                    .display = display,
                    .detector = detectorInfo.detector,
                };

                // Register the new display's brightness as having a 1.0 multiplier
                // (i.e. no dimming) and mark the first detector's displays as managed.
                // This may still be problematic if the display was previously dimmed or
                // explicitly exempt, but we can't do better without stable display IDs and
                // configuration stored across rebooting/hotplugging (we may implement this later).
                info.isManaged = firstSupportedDetector == detectorInfo.detector; // see further up
                info.brightnessLogic.setValueRange(display->knownSafeMinBrightness(), display->maxBrightness());
                info.brightnessLogic.setValue(display->brightness());

                addedDisplayIds.append(displayId);
            }
        }
    }

    // remove displays that were in the list before, but disappeared after the update
    for (const QString &displayId : m_displaysById.keys()) {
        if (!m_sortedDisplayIds.contains(displayId)) {
            const auto it = m_displaysById.constFind(displayId);
            disconnect(it->display,
                       &DisplayBrightness::externalBrightnessChangeObserved,
                       this,
                       &ScreenBrightnessController::onExternalBrightnessChangeObserved);
            m_displaysById.erase(it);

            Q_EMIT displayRemoved(displayId);
        }
    }

    // also emit signals about new and changed displays, and restore previous brightness if needed
    for (const QString &displayId : std::as_const(addedDisplayIds)) {
        Q_EMIT displayAdded(displayId);
    }
    // un-dim whenever a new display is added, also to keep dimming consistent across displays
    if (!addedDisplayIds.isEmpty()) {
        setBrightnessMultiplier(1.0);
    }
    for (const QString &displayId : std::as_const(restoreBrightnessDisplayIds)) {
        const auto it = m_displaysById.constFind(displayId);
        qCDebug(POWERDEVIL) << "Restoring screen brightness of display" << displayId << "to previous value";
        setBrightness(displayId, it->brightnessLogic.info().value);
    }
    for (const QString &displayId : std::as_const(brightnessChangedDisplayIds)) {
        const auto it = m_displaysById.constFind(displayId);
        qCDebug(POWERDEVIL) << "Screen brightness of display" << displayId << "after detection/reconfiguration:" << it->brightnessLogic.info().value;
        Q_EMIT brightnessInfoChanged(displayId, it->brightnessLogic.info());
    }

    if (!isSupported()) {
        qCDebug(POWERDEVIL) << "No suitable displays detected. Brightness controls are unsupported in this configuration.";
    }

    // legacy API needs to emit a brightness change signal for the first display, regardless of
    // whether the new first display was newly added or an existing display moved up to index 0
    const QString newFirstDisplayId = m_sortedDisplayIds.value(0, QString());
    if (!newFirstDisplayId.isEmpty() && newFirstDisplayId != previousFirstDisplayId) {
        const auto it = m_displaysById.constFind(newFirstDisplayId);
        qCDebug(POWERDEVIL) << "Screen brightness of first display after detection/reconfiguration:" << it->brightnessLogic.info().value;
        Q_EMIT legacyBrightnessInfoChanged(it->brightnessLogic.info());
    }
}

int ScreenBrightnessController::knownSafeMinBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        int result = it->display->knownSafeMinBrightness();
        qCDebug(POWERDEVIL) << "Screen knownSafeMinBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen knownSafeMinBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::minBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        int result = it->brightnessLogic.info().valueMin;
        qCDebug(POWERDEVIL) << "Screen minBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen minBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::maxBrightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        int result = it->brightnessLogic.info().valueMax;
        qCDebug(POWERDEVIL) << "Screen maxBrightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen maxBrightness failed: no display with id" << displayId;
    return 0;
}

int ScreenBrightnessController::brightness(const QString &displayId) const
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        int result = it->brightnessLogic.info().value;
        qCDebug(POWERDEVIL) << "Screen brightness of" << displayId << "is" << result;
        return result;
    }
    qCWarning(POWERDEVIL) << "Query screen brightness failed: no display with id" << displayId;
    return 0;
}

bool ScreenBrightnessController::isBrightnessManaged(const QString &displayId) const
{
    if (auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        return it->isManaged;
    }
    return false;
}

void ScreenBrightnessController::setBrightnessManaged(const QString &displayId, bool isManaged)
{
    if (auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        if (it->isManaged != isManaged) {
            it->isManaged = isManaged;
            qCWarning(POWERDEVIL) << "Set display as" << (isManaged ? "managed" : "exempt from multi-display brightness operations");
            Q_EMIT brightnessManagedChanged(displayId, isManaged);
        }
        return;
    }
    qCWarning(POWERDEVIL) << "Set display as" << (isManaged ? "managed" : "exempt from multi-display brightness operations") << "failed: no display with id"
                          << displayId;
}

void ScreenBrightnessController::setBrightness(const QString &displayId, int value)
{
    if (auto it = m_displaysById.find(displayId); it != m_displaysById.end()) {
        const PowerDevil::BrightnessLogic::BrightnessInfo bi = it->brightnessLogic.info();
        const int boundedValue = qBound(bi.valueMin, value, bi.valueMax);

        qCDebug(POWERDEVIL) << "Set screen brightness of" << displayId << "to" << boundedValue << "/" << bi.valueMax;
        if (value != boundedValue) {
            qCDebug(POWERDEVIL) << "- clamped from" << value;
        }

        // FIXME: is this what we want, or should we keep the screen dimmed even upon setBrightness()?
        // Treat the assigned brightness value as having a 1.0 multiplier (i.e. not dimmed), and
        // un-dim any other connected displays to avoid confusing dimmed/non-dimmed combinations.
        // Exempt the display from setBrightnessMultiplier() to avoid a duplicate setBrightness().
        bool isAlreadyManaged = it->isManaged;
        it->isManaged = false;
        setBrightnessMultiplier(1.0);
        it->isManaged = isAlreadyManaged;

        setBrightnessManaged(displayId, true);

        // notify only when the internally tracked brightness value is actually different
        if (bi.value != boundedValue) {
            it->brightnessLogic.setValue(boundedValue);
            Q_EMIT brightnessInfoChanged(displayId, it->brightnessLogic.info());

            // legacy API without displayId parameter: notify only if the first supported display changed
            if (displayId == m_sortedDisplayIds.first()) {
                Q_EMIT legacyBrightnessInfoChanged(it->brightnessLogic.info());
            }
        }

        // but always call setBrightness() on the display, in case we're unaware of an external change
        it->display->setBrightness(boundedValue);
    } else {
        qCWarning(POWERDEVIL) << "Set screen brightness failed: no display with id" << displayId;
    }
}

int ScreenBrightnessController::brightnessSteps(const QString &displayId)
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        return it->brightnessLogic.steps();
    }
    qCWarning(POWERDEVIL) << "Query screen brightnessSteps failed: no display with id" << displayId;
    return 1;
}

void ScreenBrightnessController::setBrightnessMultiplier(float multiplier)
{
    float boundedMultiplier = qBound(0.0, multiplier, 1.0);
    if (m_brightnessMultiplier == boundedMultiplier) {
        return;
    }
    m_brightnessMultiplier = boundedMultiplier;
    Q_EMIT brightnessMultiplierChanged(m_brightnessMultiplier);

    for (auto it = m_displaysById.constBegin(); it != m_displaysById.constEnd(); ++it) {
        if (!it->isManaged) {
            continue;
        }
        const PowerDevil::BrightnessLogic::BrightnessInfo stored = it->brightnessLogic.info();
        const int boundedValue = qMax<int>(stored.valueMin, stored.value * m_brightnessMultiplier);

        qCDebug(POWERDEVIL) << "Screen brightness multiplier" << m_brightnessMultiplier << "- adjusting" << it.key() << "to" << boundedValue << "/"
                            << stored.valueMax;
        it->display->setBrightness(boundedValue);
    }
}

float ScreenBrightnessController::brightnessMultiplier()
{
    return m_brightnessMultiplier;
}

void ScreenBrightnessController::onExternalBrightnessChangeObserved(DisplayBrightness *display, int value)
{
    const auto it = std::find_if(m_displaysById.begin(), m_displaysById.end(), [display](const DisplayInfo &info) {
        return info.display == display;
    });
    if (it == m_displaysById.end()) {
        qCWarning(POWERDEVIL) << "External brightness change of untracked display" << display->id() << "to" << value << "/" << display->maxBrightness();
        return;
    }

    qCDebug(POWERDEVIL) << "External brightness change of display" << it.key() << "to" << value << "/" << it->brightnessLogic.info().valueMax;

    // if an external actor changes display brightness, we relinquish ownership of brightness to them
    // (while treating the observed brightness as having a 1.0 multiplier, i.e. not dimmed)
    setBrightnessManaged(it.key(), false);
    it->brightnessLogic.setValue(value);

    Q_EMIT brightnessInfoChanged(it.key(), it->brightnessLogic.info());

    // legacy API without displayId parameter: notify only if the first supported display changed
    if (it.key() == m_sortedDisplayIds.first()) {
        Q_EMIT legacyBrightnessInfoChanged(it->brightnessLogic.info());
    }
}

int ScreenBrightnessController::calculateNextBrightnessStep(const QString &displayId, PowerDevil::BrightnessLogic::BrightnessKeyType keyType)
{
    if (const auto it = m_displaysById.constFind(displayId); it != m_displaysById.constEnd()) {
        return it->brightnessLogic.action(keyType);
    }
    qCWarning(POWERDEVIL) << "Calculate next brightness step failed: no display with id" << displayId;
    return -1;
}

int ScreenBrightnessController::screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type)
{
    if (!isSupported()) {
        return -1; // ignore as we are not able to determine the brightness level
    }

    int newBrightness = calculateNextBrightnessStep(m_sortedDisplayIds.first(), type);
    if (newBrightness < 0) {
        return -1;
    }

    setBrightness(m_sortedDisplayIds.first(), newBrightness);
    return newBrightness;
}

//
// legacy API without displayId parameter

int ScreenBrightnessController::knownSafeMinBrightness() const
{
    return knownSafeMinBrightness(m_sortedDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::minBrightness() const
{
    return minBrightness(m_sortedDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::maxBrightness() const
{
    return maxBrightness(m_sortedDisplayIds.value(0, QString()));
}

int ScreenBrightnessController::brightness() const
{
    return brightness(m_sortedDisplayIds.value(0, QString()));
}

void ScreenBrightnessController::setBrightness(int value)
{
    if (!m_sortedDisplayIds.isEmpty()) {
        qCWarning(POWERDEVIL) << "Set screen brightness failed: no supported display available";
    }
    // For backwards compatibility with legacy API clients, set the same brightness to all displays
    // of the first detector. i.e. either to only the backlight display, or to all external DDC monitors
    DisplayBrightnessDetector *firstSupportedDetector = nullptr;
    int firstMaxBrightness = 1;

    for (const QString &displayId : std::as_const(m_sortedDisplayIds)) {
        const auto it = m_displaysById.constFind(displayId);
        int perDisplayValue = value;

        if (firstSupportedDetector == nullptr) {
            // first display (reference for calculations)
            firstSupportedDetector = it->detector;
            firstMaxBrightness = it->display->maxBrightness();
        } else if (firstSupportedDetector == it->detector) {
            // scale the value to the other display's brightness range -
            // some displays' brightness values are ridiculously high, and can easily overflow during computation
            const qint64 newBrightness64 =
                static_cast<qint64>(value) * static_cast<qint64>(it->display->maxBrightness()) / static_cast<qint64>(firstMaxBrightness);
            // cautiously truncate it back
            perDisplayValue = static_cast<int>(qMin<qint64>(std::numeric_limits<int>::max(), newBrightness64));
        } else {
            break;
        }

        // also mark the display as managed again if it was exempt
        setBrightness(displayId, perDisplayValue);
    }
}

int ScreenBrightnessController::brightnessSteps()
{
    return brightnessSteps(m_sortedDisplayIds.value(0, QString()));
}

#include "moc_screenbrightnesscontroller.cpp"
