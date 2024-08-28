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
    enum class StepAction : uint {
        Increase = 0,
        Decrease = 1,
        IncreaseSmall = 2,
        DecreaseSmall = 3,
    };
    Q_ENUM(StepAction)

    explicit ScreenBrightnessControl(QObject *parent = nullptr);
    ~ScreenBrightnessControl() override;

    ScreenBrightnessDisplayModel *displays();
    const ScreenBrightnessDisplayModel *displays() const;

    QBindable<bool> bindableIsBrightnessAvailable();

    /**
     * Adjust brightness up or down for a predetermined set of displays by a given ratio.
     *
     * @param delta A ratio between -1.0 and +1.0 of maximum brightness.
     */
    Q_INVOKABLE void adjustBrightnessRatio(double delta);

    /**
     * Adjust brightness up or down for a predetermined set of displays by a given step action.
     *
     * @param stepAction Size and direction of the brightness adjustment. Resulting brightness will be clamped.
     */
    Q_INVOKABLE void adjustBrightnessStep(StepAction stepAction);

public Q_SLOTS:
    void setBrightness(const QString &displayId, int value);

Q_SIGNALS:
    void isBrightnessAvailableChanged(bool status);

private Q_SLOTS:
    void onBrightnessChanged(const QString &displayId, int value, const QString &sourceClientName, const QString &sourceClientContext);
    void onBrightnessRangeChanged(const QString &displayId, int max, int value);
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