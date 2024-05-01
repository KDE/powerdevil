/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#include <utility> // std::pair

#include <powerdevilcore_export.h>

#include "displaybrightness.h"
#include "powerdevilscreenbrightnesslogic.h"

class BacklightDetector;
class DDCutilDetector;

class POWERDEVILCORE_EXPORT ScreenBrightnessController : public QObject
{
    Q_OBJECT
public:
    ScreenBrightnessController();

    /**
     * Actively look for connected displays.
     *
     * This will emit detectionFinished signal once all candidates were checked, and after initial
     * detection, displaysChanged can be emitted whenever a change in display connections is observed.
     */
    void detectDisplays();
    bool isSupported() const;

    /**
     * A list of string identifiers for displays that support brightness operations.
     *
     * Note that a displayId is not necessarily stable across reboots and hotplugging events.
     * The same display may receive a different displayId when it is removed and later re-added.
     */
    QStringList displayIds() const;

    int knownSafeMinBrightness(const QString &displayId) const;
    int minBrightness(const QString &displayId) const;
    int maxBrightness(const QString &displayId) const;
    int brightness(const QString &displayId) const;

    /**
     * Return `true` if this display will be affected by multi-display brightness operations, `false` if exempt.
     *
     * Certain operations can affect more than one display. Changing brightness across the board can
     * result in tricky edge cases if not paired with an ability to store and restore brightness
     * values across reboots and hotplugging events.
     *
     * An example of something that could go wrong is the following scenario:
     * - User plugs in an external monitor, displayAdded() gets emitted.
     * - Dimming reduces its brightness to 30% of the original value.
     * - User unplugs while dimmed, displayRemoved() gets emitted and we forget about this monitor.
     * - Dimming is stopped, brightness of all remaining monitors goes back to 100%.
     * - User plugs in the same monitor, which is still at 30% of its original brightness.
     * - We forgot about it, so we don't know that brightness should be restored to its original value.
     *
     * Furthermore, a user might want to use third-party scripts or applets to control brightness
     * of some of their displays.
     *
     * To deal with such issues as good as possible, each @p displayId can be defined as managed,
     * which means its brightness will be affected by multi-display brightness operations.
     * If a display is not managed, it is exempt from such operations.
     *
     * By default, we initialize the same displays as managed that were affected by the legacy API
     * without @p displayId parameter. This should minimize regressions when moving to per-display
     * calls, and allows any edge cases to be resolved with a legacy setBrightness() call.
     *
     * A given display will switch from exempt to managed after any of the following:
     * - An explicit call to setBrightnessManaged() with `isManaged` set to true.
     * - A call to setBrightness() for the given @p displayId.
     * - A call to legacy setBrightness(), which (re)sets the default set of displays as managed.
     * - An external brightness change was observed that did not get issued by this controller.
     *
     * This controller may attempt to remember and restore displays during its runtime, including
     * the brightness managed flag, but will not persist any configuration.
     *
     * @see setBrightnessManaged
     * @see brightnessManagedChanged
     */
    bool isBrightnessManaged(const QString &displayId) const;
    /**
     * Manually specify whether this display will be affected by multi-display brightness operations.
     *
     * @see isBrightnessManaged
     * @see brightnessManagedChanged
     */
    void setBrightnessManaged(const QString &displayId, bool isManaged);

    /**
     * Set display brightness for this @p displayId to the given @p value.
     *
     * The @p value will be clamped to the range within minBrightness(@p displayId) and
     * maxBrightness(@p displayId). The display will be set as managed.
     *
     * @see setBrightnessManaged
     */
    void setBrightness(const QString &displayId, int value);
    int brightnessSteps(const QString &displayId);

    int screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type);

    // legacy API without displayId parameter, kept for backward compatibility
    int knownSafeMinBrightness() const;
    int minBrightness() const;
    int maxBrightness() const;
    int brightness() const;
    void setBrightness(int value);
    int brightnessSteps();

Q_SIGNALS:
    void detectionFinished();
    void displayAdded(const QString &displayId);
    void displayRemoved(const QString &displayId);
    void brightnessManagedChanged(const QString &displayId, bool isManaged);
    void brightnessInfoChanged(const QString &displayId, const PowerDevil::BrightnessLogic::BrightnessInfo &);

    // legacy API without displayId parameter, kept for backward compatibility.
    // include legacy prefix to avoid function overload errors when used in connect()
    void legacyBrightnessInfoChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &);

private:
    int calculateNextBrightnessStep(const QString &displayId, PowerDevil::BrightnessLogic::BrightnessKeyType keyType);

private Q_SLOTS:
    void onDetectorDisplaysChanged();
    void onExternalBrightnessChangeObserved(DisplayBrightness *display, int value);

private:
    struct DisplayInfo {
        DisplayBrightness *display = nullptr;
        DisplayBrightnessDetector *detector = nullptr;
        PowerDevil::ScreenBrightnessLogic brightnessLogic = {};
        bool isManaged = false;
    };
    QStringList m_sortedDisplayIds;
    QHash<QString, DisplayInfo> m_displaysById;

    struct DetectorInfo {
        DisplayBrightnessDetector *detector = nullptr;
        const char *debugName = nullptr;
        const char *displayIdPrefix = nullptr;
    };
    QList<DetectorInfo> m_detectors;
    int m_finishedDetectingCount = 0;
};
