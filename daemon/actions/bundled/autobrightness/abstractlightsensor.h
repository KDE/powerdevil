/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#pragma once

#include <QBindable>
#include <QObject>

namespace PowerDevil::BundledActions
{
class AbstractLightSensor : public QObject
{
    Q_OBJECT

public:
    explicit AbstractLightSensor(QObject *parent = nullptr);
    ~AbstractLightSensor() override;

    /**
     * Whether a supported light sensor of this kind is present on the system.
     */
    bool isValid() const;

    /**
     * Whether to enable automatic brightness
     */
    bool enabled() const;
    QBindable<bool> enabled();

    /**
     * The ambient light sensor reading. The unit is the same as screen
     * brightness percent (0.0~1.0).
     */
    double lightLevel() const;

Q_SIGNALS:
    void isValidChanged(bool valid);
    void lightLevelChanged(double lightLevel);

protected:
    double m_lightLevel = 0.0;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractLightSensor, bool, m_isValid, false, &AbstractLightSensor::isValidChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(AbstractLightSensor, bool, m_enabled, false)
};
}
