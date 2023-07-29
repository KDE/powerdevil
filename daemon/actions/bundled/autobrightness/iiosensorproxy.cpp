/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    Time-weighted constant for moving average:
    SPDX-FileCopyrightText: 2015 Richard Hughes <richard@hughsie.com>
    SPDX-FileCopyrightText: 2020 Kyle Hofmann <kyle.hofmann@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "iiosensorproxy.h"

#include <numbers>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

#include "dbuspropertiesinterface.h"
#include "iiosensorproxyinterface.h"
#include "powerdevil_debug.h"

using namespace PowerDevil::BundledActions;

namespace
{
inline constexpr QLatin1String SENSOR_PROXY_INTERFACE{"net.hadess.SensorProxy"};
inline constexpr QLatin1String SENSOR_PROXY_PATH{"/net/hadess/SensorProxy"};
}

IIOSensorProxy::IIOSensorProxy(QObject *parent)
    : AbstractLightSensor(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(SENSOR_PROXY_INTERFACE, QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForOwnerChange, this))
{
    m_isValid.setBinding([this] {
        return m_hasAmbientLight.value() && m_unit.value() == Vendor;
    });
    m_effectiveHasAmbientLight.setBinding([this] {
        return m_isValid.value() && m_enabled.value();
    });
    m_effectiveHasAmbientLightNotifier = m_effectiveHasAmbientLight.addNotifier([this] {
        if (m_effectiveHasAmbientLight.value()) {
            startMonitoring();
        } else {
            stopMonitoring();
        }
    });

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(SENSOR_PROXY_INTERFACE)) {
        createInterface();
    }
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &IIOSensorProxy::onServiceOwnerChanged);
}

IIOSensorProxy::~IIOSensorProxy()
{
    stopMonitoring();
}

void IIOSensorProxy::onGetAllFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QVariantMap> propsReply = *watcher;
    watcher->deleteLater();

    if (propsReply.isError()) {
        qCWarning(POWERDEVIL) << "Failed to fetch properties from" << SENSOR_PROXY_INTERFACE;
        return;
    }

    const QVariantMap props = propsReply.value();
    if (props.empty()) {
        qCWarning(POWERDEVIL) << "Failed to fetch properties from" << SENSOR_PROXY_INTERFACE;
        return;
    }

    onPropertiesChanged({}, props, {});
}

void IIOSensorProxy::onServiceOwnerChanged(const QString &serviceName, const QString &, const QString &newOwner)
{
    if (serviceName != SENSOR_PROXY_INTERFACE) {
        return;
    }

    if (m_interface) {
        qCDebug(POWERDEVIL) << "iio-sensor-proxy service" << serviceName << "just went offline";
        m_hasAmbientLight = false;
        delete m_interface;
        delete m_propsInterface;
    }

    if (!newOwner.isEmpty()) {
        qCDebug(POWERDEVIL) << "iio-sensor-proxy service" << serviceName << "just came online";
        createInterface();
    }
}

void IIOSensorProxy::onPropertiesChanged(const QString &, const QVariantMap &changedProperties, const QStringList &)
{
    auto it = changedProperties.constFind(QStringLiteral("LightLevelUnit"));
    if (it != changedProperties.cend()) {
        m_unit = it->toString() == QLatin1String("vendor") ? Vendor : Lux;

        if (m_unit == Lux) {
            qCWarning(POWERDEVIL) << "FIXME! Setting brightness from ambient lux is not implemented";
        }
    }

    it = changedProperties.constFind(QStringLiteral("HasAmbientLight"));
    if (it != changedProperties.cend()) {
        m_hasAmbientLight = it->toBool();
    }

    it = changedProperties.constFind(QStringLiteral("LightLevel"));
    if (it != changedProperties.cend()) {
        if (const auto currentLightLevelTimeStamp = QDateTime::currentSecsSinceEpoch(); currentLightLevelTimeStamp != m_smoothedLightLevelTimestamp) {
            m_rawLightLevel = std::clamp(it->toDouble(), 0.0, 1.0);

            /* Calculate exponential weighted moving average */
            double alpha = 1.0; /* Time-weighted constant for moving average */
            if (m_smoothedLightLevelTimestamp > 0) {
                /* The bandwidth of the low-pass filter used to smooth ambient light readings,
                 * measured in Hz. Smaller numbers result in smoother backlight changes.
                 * Larger numbers are more responsive to abrupt changes in ambient light. */
                constexpr double AMBIENT_BANDWIDTH_HZ = 0.1;
                constexpr double AMBIENT_TIME_CONSTANT = 1.0 / (2.0 * std::numbers::pi * AMBIENT_BANDWIDTH_HZ);
                alpha = 1.0 / (1.0 + (AMBIENT_TIME_CONSTANT / double(currentLightLevelTimeStamp - m_smoothedLightLevelTimestamp)));
            }

            if (const double smoothedLightLevel = (alpha * m_rawLightLevel) + (1.0 - alpha) * m_lightLevel; smoothedLightLevel >= 0.0) {
                m_lightLevel = smoothedLightLevel;
            } else {
                m_lightLevel = m_rawLightLevel;
            }
            Q_EMIT lightLevelChanged(m_lightLevel);

            m_smoothedLightLevelTimestamp = currentLightLevelTimeStamp;
        }
    }
}

void IIOSensorProxy::createInterface()
{
    Q_ASSERT(!m_interface && !m_propsInterface);

    m_interface = new NetHadessSensorProxyInterface(SENSOR_PROXY_INTERFACE, SENSOR_PROXY_PATH, QDBusConnection::systemBus(), this);
    m_propsInterface = new OrgFreedesktopDBusPropertiesInterface(SENSOR_PROXY_INTERFACE, SENSOR_PROXY_PATH, QDBusConnection::systemBus(), this);

    connect(m_propsInterface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &IIOSensorProxy::onPropertiesChanged);

    // Initialize properties
    QDBusPendingCall async = m_propsInterface->GetAll(SENSOR_PROXY_INTERFACE);
    auto callWatcher = new QDBusPendingCallWatcher(async, this);
    connect(callWatcher, &QDBusPendingCallWatcher::finished, this, &IIOSensorProxy::onGetAllFinished);
}

void IIOSensorProxy::startMonitoring()
{
    Q_ASSERT(m_isValid.value() && m_enabled.value());
    m_interface->ClaimLight();
}

void IIOSensorProxy::stopMonitoring()
{
    if (m_interface /* Can be nullptr when the service is gone */) {
        m_interface->ReleaseLight();
    }

    m_smoothedLightLevelTimestamp = 0;
}

#include "moc_iiosensorproxy.cpp"