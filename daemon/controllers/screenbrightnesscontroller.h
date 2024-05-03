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

    /**
     * A human-readable name for this display.
     */
    QString label(const QString &label) const;

    int knownSafeMinBrightness(const QString &displayId) const;
    int minBrightness(const QString &displayId) const;
    int maxBrightness(const QString &displayId) const;
    int brightness(const QString &displayId) const;

    /**
     * Set display brightness for this @p displayId to the given @p value.
     *
     * The @p value will be clamped to the range within minBrightness(@p displayId) and
     * maxBrightness(@p displayId). @p sourceClientName and @p sourceClientContext are passed to
     * the `brightnessChanged` signal that is emitted when a change has indeed happened.
     *
     * If the display is affected by the brightness multiplier and a multiplier other than 1.0
     * is currently set, it will be applied before clamping.
     *
     * @see setBrightnessMultiplier
     * @see brightnessChanged
     */
    void setBrightness(const QString &displayId, int value, const QString &sourceClientName, const QString &sourceClientContext);

    int brightnessSteps(const QString &displayId);

    /**
     * Modify the brightness of all affected displays with a @p multiplier between 0.0 and 1.0.
     *
     * The set of affected displays may change over time. For now, it refers to the set of
     * legacy displays that can also be controlled with legacy setBrightness() without displayId
     * parameter.
     *
     * If brightness changes are observed from an external source, the multiplier will be reset
     * to 1.0.
     *
     * Setting this multiplier will have the same visible effect as calling setBrightness()
     * for all affected displays with the brightness value multiplied by @p multiplier. However,
     * unlike setBrightness(), this function will not affect the value returned by brightness().
     * Setting the multiplier to 1.0 will restore the original brightness.
     *
     * Multiplier arguments outside of 0.0 and 1.0 will be clamped to this range.
     *
     * @see legacyDisplayIds
     * @see brightnessMultiplierChanged
     */
    void setBrightnessMultiplier(float multiplier);
    float brightnessMultiplier();

    int screenBrightnessKeyPressed(PowerDevil::BrightnessLogic::BrightnessKeyType type);

    // legacy API without displayId parameter, kept for backward compatibility
    QStringList legacyDisplayIds() const;
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
    void brightnessChanged(const QString &displayId,
                           const PowerDevil::BrightnessLogic::BrightnessInfo &,
                           const QString &sourceClientName,
                           const QString &sourceClientContext);
    void brightnessMultiplierChanged(float multiplier);

    // legacy API without displayId parameter, kept for backward compatibility.
    // include legacy prefix to avoid function overload errors when used in connect()
    void legacyDisplayIdsChanged(const QStringList &);
    void legacyBrightnessInfoChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &);

private:
    int brightnessMultiplied(int value, bool usesMultiplier, int min) const;
    int calculateNextBrightnessStep(const QString &displayId, PowerDevil::BrightnessLogic::BrightnessKeyType keyType);

private Q_SLOTS:
    void onDisplayDestroyed(QObject *);
    void onDetectorDisplaysChanged();
    void onExternalBrightnessChangeObserved(DisplayBrightness *display, int value);

private:
    struct DisplayInfo {
        DisplayBrightness *display = nullptr;
        DisplayBrightnessDetector *detector = nullptr;
        PowerDevil::ScreenBrightnessLogic brightnessLogic = {};
        bool usesMultiplier = false;
        bool zombie = false;
    };
    QStringList m_sortedDisplayIds;
    QStringList m_legacyDisplayIds;
    QHash<QString, DisplayInfo> m_displaysById;

    struct DetectorInfo {
        DisplayBrightnessDetector *detector = nullptr;
        const char *debugName = nullptr;
        const char *displayIdPrefix = nullptr;
    };
    QList<DetectorInfo> m_detectors;
    int m_finishedDetectingCount = 0;

    float m_brightnessMultiplier = 1.0;
};
