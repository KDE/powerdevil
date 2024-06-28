/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "screenbrightnessdisplaymodel.h"

#include <QBindable>
#include <QCoroTask>
#include <QObject>
#include <qqmlregistration.h>

class QDBusPendingCallWatcher;

class ScreenBrightnessControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractListModel *displays READ displays CONSTANT)

    Q_PROPERTY(bool isBrightnessAvailable READ default NOTIFY isBrightnessAvailableChanged BINDABLE bindableIsBrightnessAvailable)
    Q_PROPERTY(bool isSilent MEMBER m_isSilent)

public:
    explicit ScreenBrightnessControl(QObject *parent = nullptr);
    ~ScreenBrightnessControl() override;

    ScreenBrightnessDisplayModel *displays();
    const ScreenBrightnessDisplayModel *displays() const;

    QBindable<bool> bindableIsBrightnessAvailable();

    /**
     * Adjust brightness up or down for all displays by a ratio between -1.0 and +1.0 of maximum brightness.
     *
     * @param delta Brightness adjustment ratio. Resulting brightness will be clamped.
     * @param stepCount Snap brightness to this number of steps.
     * @param trackingErrorsByDisplayId Continue from a previously returned set of tracking errors.
     *
     * @return The set of tracking errors (brightness value fraction requested, but not applied)
     *         of all adjusted displays. Can be used to smooth out repeat calls.
     */
    Q_INVOKABLE QHash<QString, float> changeBrightnessByDelta(float delta, const QHash<QString, float> &trackingErrorsByDisplayId = {});

    /**
     * Adjust brightness up or down for all displays by a given number of steps, snapping to the
     * closest step value.
     *
     * @param stepDelta Number of steps to adjust brightness. Resulting brightness will be clamped.
     * @param stepCount The number of steps to subdivide brightness into.
     *
     * @return The set of tracking errors (brightness value fraction requested, but not applied)
     *         of all adjusted displays. Can be used to smooth out repeat calls.
     */
    Q_INVOKABLE void changeBrightnessBySteps(float stepDelta, int stepCount);

public Q_SLOTS:
    void setBrightness(const QString &displayId, int value);

Q_SIGNALS:
    void isBrightnessAvailableChanged(bool status);

private Q_SLOTS:
    void onBrightnessChanged(const QString &displayId, int value, const QString &sourceClientName = QString(), const QString &sourceClientContext = QString());
    void onBrightnessRangeChanged(const QString &displayId, int min, int max, int value);
    void onDisplayAdded(const QString &displayId);
    void onDisplayRemoved(const QString &displayId);

private:
    QCoro::Task<void> init();
    QCoro::Task<void> queryAndAppendDisplay(const QString &displayId);

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(ScreenBrightnessControl, bool, m_isBrightnessAvailable, false, &ScreenBrightnessControl::isBrightnessAvailableChanged);

    ScreenBrightnessDisplayModel m_displays;
    std::unique_ptr<QDBusPendingCallWatcher> m_brightnessChangeWatcher;

    bool m_isSilent = false;
};
