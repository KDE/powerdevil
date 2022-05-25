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

#pragma once

#include <powerdevilaction.h>

class OrgFreedesktopDBusPropertiesInterface;
class NetHadessPowerProfilesInterface;

namespace PowerDevil
{
namespace BundledActions
{
class PowerProfile : public PowerDevil::Action, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerProfile)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.PowerProfile")

    Q_PROPERTY(QStringList profileChoices READ profileChoices NOTIFY profileChoicesChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY currentProfileChanged)

public:
    explicit PowerProfile(QObject *parent, const QVariantList &);
    ~PowerProfile() override;

    bool loadAction(const KConfigGroup &config) override;

    QStringList profileChoices() const;
    QString currentProfile() const;
    void setProfile(const QString &profile);
    QString performanceInhibitedReason() const;
    QString performanceDegradedReason() const;
    QList<QVariantMap> profileHolds() const;
    unsigned int holdProfile(const QString &profile, const QString &reason, const QString &applicationId);
    void releaseProfile(unsigned int cookie);

Q_SIGNALS:
    void currentProfileChanged(const QString &profile);
    void profileChoicesChanged(const QStringList &profiles);
    void performanceInhibitedReasonChanged(const QString &reason);
    void performanceDegradedReasonChanged(const QString &reason);
    void profileHoldsChanged(const QList<QVariantMap> &holds);

protected:
    void onProfileLoad() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileUnload() override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;
private Q_SLOTS:
    void propertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    void readProperties(const QVariantMap &properties);
    void serviceUnregistered(const QString &name);

    NetHadessPowerProfilesInterface *m_powerProfilesInterface;
    OrgFreedesktopDBusPropertiesInterface *m_powerProfilesPropertiesInterface;
    QStringList m_profileChoices;
    QString m_currentProfile;
    QString m_performanceInhibitedReason;
    QString m_degradationReason;
    QList<QVariantMap> m_profileHolds;
    QDBusServiceWatcher *m_holdWatcher;
    QMultiMap<QString, unsigned int> m_holdMap;

    QString m_configuredProfile;
};

} // namespace BundledActions
} // namespace PowerDevil
