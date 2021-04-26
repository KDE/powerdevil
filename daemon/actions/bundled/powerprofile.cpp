/*
 * Copyright 2020 Kai Uwe Broulik <kde@broulik.de>
 * Copyright 2021 David Redondo <kde@david-redondo.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "powerprofile.h"

#include "power_profiles_interface.h"
#include "powerprofileadaptor.h"
#include "properties_interface.h"

#include <powerdevil_debug.h>

#include <KConfigGroup>

using namespace PowerDevil::BundledActions;

static const QString activeProfileProperty = QStringLiteral("ActiveProfile");
static const QString profilesProperty = QStringLiteral("Profiles");
static const QString performanceInhibitedProperty = QStringLiteral("PerformanceInhibited");
static const QString performanceDegradedProperty = QStringLiteral("PerformanceDegraded");

static const QString ppdName = QStringLiteral("net.hadess.PowerProfiles");
static const QString ppdPath = QStringLiteral("/net/hadess/PowerProfiles");

PowerProfile::PowerProfile(QObject *parent)
    : Action(parent)
    , m_powerProfilesInterface(new NetHadessPowerProfilesInterface(ppdName, ppdPath, QDBusConnection::systemBus(), this))
    , m_powerProfilesPropertiesInterface(new OrgFreedesktopDBusPropertiesInterface(ppdName, ppdPath, QDBusConnection::systemBus(), this))
{
    new PowerProfileAdaptor(this);

    connect(m_powerProfilesPropertiesInterface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged, this, &PowerProfile::propertiesChanged);

    auto watcher = new QDBusPendingCallWatcher(m_powerProfilesPropertiesInterface->GetAll(m_powerProfilesInterface->interface()));
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *watcher;
        if (watcher->isError()) {
            return;
        }
        readProperties(reply.value());
    });
}

PowerProfile::~PowerProfile() = default;

void PowerProfile::onProfileLoad()
{
    if (!m_configuredProfile.isEmpty()) {
        setProfile(m_configuredProfile);
    }
}

void PowerProfile::onWakeupFromIdle()
{
}

void PowerProfile::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void PowerProfile::onProfileUnload()
{
}

void PowerProfile::triggerImpl(const QVariantMap &args)
{
    Q_UNUSED(args);
}

bool PowerProfile::loadAction(const KConfigGroup &config)
{
    if (config.hasKey("profile")) {
        m_configuredProfile = config.readEntry("profile", QString());
    }

    return true;
}

bool PowerProfile::isSupported()
{
    return QDBusConnection::systemBus().interface()->activatableServiceNames().value().contains(ppdName);
}

QStringList PowerProfile::profileChoices() const
{
    return m_profileChoices;
}

QString PowerProfile::currentProfile() const
{
    return m_currentProfile;
}

QString PowerProfile::performanceDegradedReason() const
{
    return m_degradationReason;
}

QString PowerProfile::performanceInhibitedReason() const
{
    return m_performanceInhibitedReason;
}

void PowerProfile::setProfile(const QString &profile)
{
    auto call = m_powerProfilesPropertiesInterface->Set(m_powerProfilesInterface->interface(), activeProfileProperty, QDBusVariant(profile));
    if (calledFromDBus()) {
        setDelayedReply(true);
        const auto msg = message();
        auto watcher = new QDBusPendingCallWatcher(call);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [msg, watcher] {
            watcher->deleteLater();
            if (watcher->isError()) {
                QDBusConnection::sessionBus().send(msg.createErrorReply(watcher->error()));
            } else {
                QDBusConnection::sessionBus().send(msg.createReply());
            }
        });
    }
}

void PowerProfile::readProperties(const QVariantMap &properties)
{
    if (properties.contains(activeProfileProperty)) {
        m_currentProfile = properties[activeProfileProperty].toString();
        Q_EMIT currentProfileChanged(m_currentProfile);
    }

    if (properties.contains(profilesProperty)) {
        QList<QVariantMap> profiles;
        properties[profilesProperty].value<QDBusArgument>() >> profiles;
        m_profileChoices.clear();
        if (profiles.first()[QStringLiteral("Driver")] != QLatin1String("placeholder")) {
            std::transform(profiles.cbegin(), profiles.cend(), std::back_inserter(m_profileChoices), [](const QVariantMap &dict) {
                return dict[QStringLiteral("Profile")].toString();
            });
        }
        Q_EMIT profileChoicesChanged(m_profileChoices);
    }

    if (properties.contains(performanceInhibitedProperty)) {
        m_performanceInhibitedReason = properties[performanceInhibitedProperty].toString();
        Q_EMIT performanceInhibitedReasonChanged(m_performanceInhibitedReason);
    }

    if (properties.contains(performanceDegradedProperty)) {
        m_degradationReason = properties[performanceDegradedProperty].toString();
        Q_EMIT performanceDegradedReasonChanged(m_degradationReason);
    }
}

void PowerProfile::propertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (interface != m_powerProfilesInterface->interface()) {
        return;
    }
    readProperties(changed);
}
