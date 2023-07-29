/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstractlightsensor.h"

#include <QPointer>

class QDBusServiceWatcher;
class QDBusPendingCallWatcher;

class NetHadessSensorProxyInterface;
class OrgFreedesktopDBusPropertiesInterface;

namespace PowerDevil::BundledActions
{
class IIOSensorProxy : public AbstractLightSensor
{
    Q_OBJECT

public:
    explicit IIOSensorProxy(QObject *parent = nullptr);
    ~IIOSensorProxy() override;

private Q_SLOTS:
    void onGetAllFinished(QDBusPendingCallWatcher *watcher);
    void onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    void onPropertiesChanged(const QString &, const QVariantMap &changedProperties, const QStringList &);

private:
    enum LightLevelUnit {
        Lux,
        Vendor,
    };

    void createInterface();
    void startMonitoring();
    void stopMonitoring();

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(IIOSensorProxy, bool, m_effectiveHasAmbientLight, false)
    QPropertyNotifier m_effectiveHasAmbientLightNotifier;

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(IIOSensorProxy, bool, m_hasAmbientLight, false)

    /**
     * The ambient light sensor reading, in the unit exported in the
     * "LightLevelUnit" property.
     */
    double m_rawLightLevel = 0.0;

    /**
     * The unit used in Ambient Light Sensor readings. It is
     * one of either "lux" or "vendor".
     *
     * @note different hardware will have different readings in the
     * same condition, so values should be taken as relative.
     *
     * When the unit is "vendor", readings will be a percentage with 100% being the
     * maximum reading.
     */
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(IIOSensorProxy, LightLevelUnit, m_unit, Lux)

    qint64 m_smoothedLightLevelTimestamp = 0;

    const QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QPointer<NetHadessSensorProxyInterface> m_interface;
    QPointer<OrgFreedesktopDBusPropertiesInterface> m_propsInterface;
};

}
