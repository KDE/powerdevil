/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KCModuleData>
#include <KQuickManagedConfigModule>

#include <QList>
#include <QQmlEngine>
#include <QVariant>

// Profile configuration for PowerDevil, i.e. separate settings for AC/Battery/LowBattery power states

class PowerButtonActionModel;
class PowerProfileModel;
class SleepModeModel;

namespace PowerDevil
{
class ProfileSettings;

class ProfileConfigData : public KCModuleData
{
    Q_OBJECT

    Q_PROPERTY(QString profileId READ profileId CONSTANT)
    Q_PROPERTY(QObject *settings READ settings CONSTANT)
    Q_PROPERTY(QObject *autoSuspendActionModel READ autoSuspendActionModel CONSTANT)
    Q_PROPERTY(QObject *powerButtonActionModel READ powerButtonActionModel CONSTANT)
    Q_PROPERTY(QObject *lidActionModel READ lidActionModel CONSTANT)
    Q_PROPERTY(QObject *sleepModeModel READ sleepModeModel CONSTANT)
    Q_PROPERTY(QObject *powerProfileModel READ powerProfileModel CONSTANT)

public:
    explicit ProfileConfigData(const QString &profileId, bool isMobile, bool isVM, bool canSuspendToRam, QObject *parent);
    ~ProfileConfigData() override;

    QString profileId() const;
    ProfileSettings *settings() const;

    QObject *autoSuspendActionModel() const;
    QObject *powerButtonActionModel() const;
    QObject *lidActionModel() const;
    QObject *sleepModeModel() const;
    QObject *powerProfileModel() const;

private:
    QString m_profileId;
    ProfileSettings *m_settings;

    PowerButtonActionModel *m_autoSuspendActionModel;
    PowerButtonActionModel *m_powerButtonActionModel;
    PowerButtonActionModel *m_lidActionModel;
    SleepModeModel *m_sleepModeModel;
    PowerProfileModel *m_powerProfileModel;
};

class ProfilesConfigKCM : public KQuickManagedConfigModule
{
    Q_OBJECT

    // e.g. {"BrightnessControl": true, "KeyboardBrightnessControl": false, "SuspendSession": true, ...}
    Q_PROPERTY(QVariantMap supportedActions READ supportedActions NOTIFY supportedActionsChanged)

    Q_PROPERTY(QVariantMap profileData READ profileData CONSTANT)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY currentProfileChanged)
    Q_PROPERTY(bool supportsBatteryProfiles READ supportsBatteryProfiles NOTIFY supportsBatteryProfilesChanged)

    Q_PROPERTY(bool powerManagementServiceRegistered READ powerManagementServiceRegistered NOTIFY powerManagementServiceRegisteredChanged)
    Q_PROPERTY(QString powerManagementServiceErrorReason READ powerManagementServiceErrorReason NOTIFY powerManagementServiceErrorReasonChanged)

public:
    ProfilesConfigKCM(QObject *parent, const KPluginMetaData &metaData);

    QVariantMap supportedActions() const;

    QVariantMap profileData() const;
    QString currentProfile() const;
    bool supportsBatteryProfiles() const;

    bool powerManagementServiceRegistered() const;
    QString powerManagementServiceErrorReason() const;

public Q_SLOTS:
    void load() override;
    void save() override;

Q_SIGNALS:
    void supportedActionsChanged();

    void currentProfileChanged();
    void supportsBatteryProfilesChanged();

    void powerManagementServiceRegisteredChanged();
    void powerManagementServiceErrorReasonChanged();

private:
    void setCurrentProfile(const QString &currentProfile);
    void setSupportsBatteryProfiles(bool);

    void setPowerManagementServiceRegistered(bool);
    void setPowerManagementServiceErrorReason(const QString &);
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

    QVariantMap m_supportedActions;

    QVariantMap m_profileData;
    QString m_currentProfile;
    bool m_supportsBatteryProfiles;

    bool m_powerManagementServiceRegistered;
    QString m_powerManagementServiceErrorReason;
};

} // namespace PowerDevil
