/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <KScreen/Types>

#include <QHash>
#include <QObject>
#include <QStringList>

#include <optional>
#include <unordered_map>

#include <powerdevilcore_export.h>

#include "displaybrightness.h"
#include "displaymatch.h"
#include "powerdevilscreenbrightnesslogic.h"

class ExternalBrightnessController;

class POWERDEVILCORE_EXPORT ScreenBrightnessController : public QObject
{
    Q_OBJECT

public:
    enum BrightnessStepSize {
        RegularStepSize,
        SmallStepSize,
    };

    /// This hint will be ignored by ScreenBrightnessController itself, but forwarded to any
    /// signals emitted by the corresponding brightness setter call.
    enum IndicatorHint {
        ShowIndicator = 0,
        SuppressIndicator = 1,
    };

    explicit ScreenBrightnessController();
    ~ScreenBrightnessController() override;

    /**
     * Actively look for connected displays.
     *
     * This will emit detectionFinished signal once all candidates were checked, and after initial
     * detection, displayAdded/displayRemoved can be emitted whenever a change in display
     * connections is observed.
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

    /**
     * Returns true if the display is part of the system (e.g. laptop panel),
     * false if external (e.g. monitor, drawing tablet) or unknown.
     */
    bool isInternal(const QString &displayId) const;

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
     * @see brightnessChanged
     */
    void setBrightness(const QString &displayId,
                       int value,
                       const QString &sourceClientName,
                       const QString &sourceClientContext,
                       IndicatorHint hint = SuppressIndicator);

    /**
     * Adjust display brightness for this @p displayId by a @p delta between -1.0 and 1.0.
     *
     * The @p delta will be converted to and clamped to the valid brightness range for this display.
     *  @p sourceClientName and @p sourceClientContext are passed to the `brightnessChanged` signal
     * that is emitted for each display when a change has indeed happened.
     *
     * @see brightnessChanged
     */
    void adjustBrightnessRatio(const QString &displayId, double delta, const QString &sourceClientName, const QString &sourceClientContext, IndicatorHint hint);

    /**
     * Adjust display brightness for a predetermined set of displays by a @p delta between -1.0 and 1.0.
     *
     * The @p delta will be converted to and clamped to the valid brightness range for each
     * affected display. @p sourceClientName and @p sourceClientContext are passed to the
     * `brightnessChanged` signal that is emitted for each display when a change has indeed happened.
     *
     * @see brightnessChanged
     */
    void adjustBrightnessRatio(double delta, const QString &sourceClientName, const QString &sourceClientContext, IndicatorHint hint = SuppressIndicator);

    /**
     * Adjust display brightness for a predetermined set of displays by a step up or down the
     * brightness scale.
     *
     * The direction and size of each step are specified by @p step. The exact behavior of step
     * movement and the set of affected displays are an implementation detail that may change
     * over time. @p sourceClientName and @p sourceClientContext are passed to the
     * `brightnessChanged` signal that is emitted when a change has indeed happened.
     *
     * @see brightnessChanged
     */
    void adjustBrightnessStep(PowerDevil::BrightnessLogic::StepAdjustmentAction adjustment,
                              const QString &sourceClientName,
                              const QString &sourceClientContext,
                              IndicatorHint hint = SuppressIndicator);

    int brightnessSteps(const QString &displayId) const;

    /**
     * Add, modify or remove a brightness limit for dimming, with a @p ratio between 0.0 and 1.0.
     *
     * Several limits can be set independently, distinguished by their @p dimmingId.
     * The lowest one will apply. Setting the @p ratio to 1.0 will remove this limit.
     *
     * Setting a dimming limit will have the same visible effect as calling setBrightness()
     * for all affected displays with the brightness value multiplied by @p ratio. However,
     * unlike setBrightness(), this function will not affect the value returned by brightness().
     * Subsequent setBrightness() calls will apply the same multiplier.
     *
     * By default, all currently connected displays are affected. Optional display id filter
     * parameters may be added in the future.
     *
     * Newly connected displays will remain at full brightness (i.e ratio of 1.0) until another
     * call of this function affects them. Displays which were previously dimmed and are
     * re-recognized after reconnection will be restored to the current applicable limit.
     */
    void setDimmingRatio(const QString &dimmingId, double ratio);

    KScreen::OutputPtr tryMatchKScreenOutput(const QString &displayId) const;

    // legacy API without displayId parameter, kept for backward compatibility
    QStringList legacyDisplayIds() const;
    int knownSafeMinBrightness() const;
    int minBrightness() const;
    int maxBrightness() const;
    int brightness() const;
    void setBrightness(int value, IndicatorHint hint = SuppressIndicator);
    int brightnessSteps() const;

Q_SIGNALS:
    void detectionFinished();
    void displayIdsChanged(const QStringList &displayIds);
    void displayAdded(const QString &displayId);
    void displayRemoved(const QString &displayId);
    void brightnessChanged(const QString &displayId,
                           const PowerDevil::BrightnessLogic::BrightnessInfo &,
                           const QString &sourceClientName,
                           const QString &sourceClientContext,
                           IndicatorHint);

    // legacy API without displayId parameter, kept for backward compatibility.
    // include legacy prefix to avoid function overload errors when used in connect()
    void legacyDisplayIdsChanged(const QStringList &);
    void legacyBrightnessInfoChanged(const PowerDevil::BrightnessLogic::BrightnessInfo &, IndicatorHint);

private Q_SLOTS:
    void onDisplayDestroyed(QObject *);
    void onDetectorDisplaysChanged();
    void onExternalBrightnessChangeObserved(DisplayBrightness *display, int value);

private:
    int brightnessMultiplied(int value, double multiplier, int min) const;
    double dimmingRatioForDisplay(const QString &displayId);

private:
    struct DisplayInfo {
        DisplayBrightness *display = nullptr;
        DisplayBrightnessDetector *detector = nullptr;
        PowerDevil::ScreenBrightnessLogic brightnessLogic = {};
        DisplayMatch match;
        double dimmingRatio = 1.0;
        double trackingError = 0.0;
        bool zombie = false;
    };
    QStringList m_sortedDisplayIds;
    QStringList m_legacyDisplayIds;
    std::unordered_map<QString, DisplayInfo> m_displaysById;

    struct DetectorInfo {
        DisplayBrightnessDetector *detector = nullptr;
        const char *debugName = nullptr;
        const char *displayIdPrefix = nullptr;
    };
    QList<DetectorInfo> m_detectors;
    int m_finishedDetectingCount = 0;
    std::unique_ptr<ExternalBrightnessController> m_externalBrightnessController;

    struct RememberedDisplayState {
        int brightness;
        int minBrightness = 0;
        std::optional<double> latestActiveDimmingRatio;
    };
    std::map<DisplayMatch, RememberedDisplayState> m_rememberedDisplayState;

    struct DimmingLimit {
        double ratio = 1.0;
    };
    std::unordered_map<QString, DimmingLimit> m_dimmingLimits;

    KScreen::ConfigPtr m_kscreenConfig = nullptr;
};
