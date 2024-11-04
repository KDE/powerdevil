/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "applicationdata_p.h"

#include <QDBusPendingCall>
#include <QObject>
#include <QObjectBindableProperty>
#include <QStringList>
#include <qqmlregistration.h>

#include <memory>

class QDBusServiceWatcher;

class PowerProfilesControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool isPowerProfileDaemonInstalled READ default NOTIFY isPowerProfileDaemonInstalledChanged BINDABLE bindableIsPowerProfileDaemonInstalled);
    Q_PROPERTY(QStringList profiles READ default NOTIFY profilesChanged BINDABLE bindableProfiles)
    Q_PROPERTY(QString configuredProfile READ default NOTIFY configuredProfileChanged BINDABLE bindableConfiguredProfile)
    Q_PROPERTY(QString activeProfile READ default NOTIFY activeProfileChanged BINDABLE bindableActiveProfile)
    Q_PROPERTY(QString profileError READ default WRITE default NOTIFY profileErrorChanged BINDABLE bindableProfileError)
    Q_PROPERTY(QString inhibitionReason READ default NOTIFY inhibitionReasonChanged BINDABLE bindableInhibitionReason)
    Q_PROPERTY(QString degradationReason READ default NOTIFY degradationReasonChanged BINDABLE bindableDegradationReason)
    Q_PROPERTY(QList<QVariantMap> profileHolds READ default NOTIFY profileHoldsChanged BINDABLE bindableProfileHolds)
    Q_PROPERTY(bool isSilent READ isSilent WRITE setIsSilent)
    Q_PROPERTY(bool isTlpInstalled READ default NOTIFY isTlpInstallChanged BINDABLE bindableIsTlpInstalled)

public:
    explicit PowerProfilesControl(QObject *parent = nullptr);
    ~PowerProfilesControl() override;

    Q_INVOKABLE void setProfile(const QString &reason);

private:
    bool isSilent();
    void setIsSilent(bool status);
    void showPowerProfileOsd(const QString &reason);
    void setTlpInstalled(bool installed);

    QBindable<bool> bindableIsPowerProfileDaemonInstalled();
    QBindable<QStringList> bindableProfiles();
    QBindable<QString> bindableConfiguredProfile();
    QBindable<QString> bindableActiveProfile();
    QBindable<QString> bindableProfileError();
    QBindable<QString> bindableInhibitionReason();
    QBindable<QString> bindableDegradationReason();
    QBindable<QList<QVariantMap>> bindableProfileHolds();
    QBindable<bool> bindableIsTlpInstalled();

private Q_SLOTS:
    void onServiceRegistered(const QString &serviceName);
    void onServiceUnregistered(const QString &serviceName);

    void updatePowerProfileChoices(const QStringList &choices);
    void updatePowerProfileConfiguredProfile(const QString &profile);
    void updatePowerProfileCurrentProfile(const QString &profile);
    void updatePowerProfilePerformanceInhibitedReason(const QString &reason);
    void updatePowerProfilePerformanceDegradedReason(const QString &reason);
    void updatePowerProfileHolds(QList<QVariantMap> holds);

Q_SIGNALS:
    void isPowerProfileDaemonInstalledChanged(bool status);
    void profilesChanged(const QStringList &profiles);
    void configuredProfileChanged(const QString &profile);
    void activeProfileChanged(const QString &profile);
    void profileErrorChanged(const QString &status);
    void inhibitionReasonChanged(const QString &reason);
    void degradationReasonChanged(const QString &reason);
    void profileHoldsChanged(const QList<QVariantMap> &profileHolds);
    void isTlpInstallChanged(bool installed);

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(PowerProfilesControl,
                                         bool,
                                         m_isPowerProfileDaemonInstalled,
                                         false,
                                         &PowerProfilesControl::isPowerProfileDaemonInstalledChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QStringList, m_profiles, &PowerProfilesControl::profilesChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QString, m_configuredProfile, &PowerProfilesControl::configuredProfileChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QString, m_activeProfile, &PowerProfilesControl::activeProfileChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QString, m_profileError, &PowerProfilesControl::profileErrorChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QString, m_inhibitionReason, &PowerProfilesControl::inhibitionReasonChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QString, m_degradationReason, &PowerProfilesControl::degradationReasonChanged)
    Q_OBJECT_BINDABLE_PROPERTY(PowerProfilesControl, QList<QVariantMap>, m_profileHolds, &PowerProfilesControl::profileHoldsChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(PowerProfilesControl, bool, m_isTlpInstalled, false, &PowerProfilesControl::isTlpInstallChanged)

    std::unique_ptr<QDBusServiceWatcher> m_solidWatcher;
    std::unique_ptr<QDBusServiceWatcher> m_powerProfileWatcher;

    bool m_isSilent = false;

    ApplicationData m_data;
};
