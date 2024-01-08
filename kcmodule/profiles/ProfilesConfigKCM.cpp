/*
 *  SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ProfilesConfigKCM.h"
#include "ExternalServiceSettings.h"

// powerdevil/kcmodule/common
#include <PowerButtonActionModel.h>
#include <PowerProfileModel.h>
#include <SleepModeModel.h>

// powerdevil/daemon
#include <PowerDevilGlobalSettings.h>
#include <PowerDevilProfileSettings.h>
#include <lidcontroller.h>
#include <powerdevilenums.h>
#include <powerdevilpowermanagement.h>

// debug category for qCDebug()
#include <powerdevil_debug.h>

// KDE
#include <KLocalizedString>
#include <KPluginFactory>
#include <Kirigami/Platform/TabletModeWatcher>
#include <Solid/Battery>
#include <Solid/Device>

// Qt
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QQuickItem>
#include <QQuickRenderControl>

K_PLUGIN_FACTORY_WITH_JSON(PowerDevilProfilesConfigFactory, "kcm_powerdevilprofilesconfig.json", {
    registerPlugin<PowerDevil::ProfilesConfigKCM>();
    registerPlugin<PowerDevil::ProfilesConfigData>();
})

namespace PowerDevil
{

ProfilesConfigData::ProfilesConfigData(QObject *parent, const KPluginMetaData &metaData)
    : ProfilesConfigData(parent,
                         Kirigami::Platform::TabletModeWatcher::self()->isTabletMode() /*isMobile*/,
                         PowerDevil::PowerManagement::instance()->isVirtualMachine() /*isVM*/,
                         PowerDevil::PowerManagement::instance()->canSuspend(),
                         PowerDevil::PowerManagement::instance()->canHibernate())
{
    Q_UNUSED(metaData);
}

ProfilesConfigData::ProfilesConfigData(QObject *parent, bool isMobile, bool isVM, bool canSuspend, bool canHibernate)
    : KCModuleData(parent)
    , m_globalSettings(new GlobalSettings(canSuspend, canHibernate, this))
    , m_settingsAC(new ProfileSettings("AC", isMobile, isVM, canSuspend, this))
    , m_settingsBattery(new ProfileSettings("Battery", isMobile, isVM, canSuspend, this))
    , m_settingsLowBattery(new ProfileSettings("LowBattery", isMobile, isVM, canSuspend, this))
{
    autoRegisterSkeletons();
}

ProfilesConfigData::~ProfilesConfigData()
{
}

GlobalSettings *ProfilesConfigData::global() const
{
    return m_globalSettings;
}

ProfileSettings *ProfilesConfigData::profileAC() const
{
    return m_settingsAC;
}

ProfileSettings *ProfilesConfigData::profileBattery() const
{
    return m_settingsBattery;
}

ProfileSettings *ProfilesConfigData::profileLowBattery() const
{
    return m_settingsLowBattery;
}

ProfilesConfigKCM::ProfilesConfigKCM(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_settings(new ProfilesConfigData(this, metaData))
    , m_externalServiceSettings(new ExternalServiceSettings(this))
    , m_supportsBatteryProfiles(false)
    , m_isPowerSupplyBatteryPresent(false)
    , m_isPeripheralBatteryPresent(false)
    , m_isLidPresent(false)
    , m_isPowerButtonPresent(false)
    , m_powerManagementServiceRegistered(false)
    , m_autoSuspendActionModel(new PowerButtonActionModel(this,
                                                          PowerManagement::instance(),
                                                          {
                                                              PowerButtonAction::NoAction,
                                                              PowerButtonAction::Sleep,
                                                              PowerButtonAction::Hibernate,
                                                              PowerButtonAction::Shutdown,
                                                          }))
    , m_batteryCriticalActionModel(new PowerButtonActionModel(this,
                                                              PowerManagement::instance(),
                                                              {
                                                                  PowerButtonAction::NoAction,
                                                                  PowerButtonAction::Sleep,
                                                                  PowerButtonAction::Hibernate,
                                                                  PowerButtonAction::Shutdown,
                                                              }))
    , m_powerButtonActionModel(new PowerButtonActionModel(this,
                                                          PowerManagement::instance(),
                                                          {
                                                              PowerButtonAction::NoAction,
                                                              PowerButtonAction::Sleep,
                                                              PowerButtonAction::Hibernate,
                                                              PowerButtonAction::Shutdown,
                                                              PowerButtonAction::LockScreen,
                                                              PowerButtonAction::PromptLogoutDialog,
                                                              PowerButtonAction::TurnOffScreen,
                                                          }))
    , m_lidActionModel(new PowerButtonActionModel(this,
                                                  PowerManagement::instance(),
                                                  {
                                                      PowerButtonAction::NoAction,
                                                      PowerButtonAction::Sleep,
                                                      PowerButtonAction::Hibernate,
                                                      PowerButtonAction::Shutdown,
                                                      PowerButtonAction::LockScreen,
                                                      PowerButtonAction::TurnOffScreen,
                                                  }))
    , m_sleepModeModel(new SleepModeModel(this, PowerManagement::instance()))
    , m_powerProfileModel(new PowerProfileModel(this))
{
    qmlRegisterUncreatableMetaObject(PowerDevil::staticMetaObject, "org.kde.powerdevil", 1, 0, "PowerDevil", QStringLiteral("For enums and flags only"));

    connect(m_externalServiceSettings, &ExternalServiceSettings::settingsChanged, this, &ProfilesConfigKCM::settingsChanged);
    connect(m_externalServiceSettings,
            &ExternalServiceSettings::isChargeStartThresholdSupportedChanged,
            this,
            &ProfilesConfigKCM::isChargeStartThresholdSupportedChanged);
    connect(m_externalServiceSettings,
            &ExternalServiceSettings::isChargeStopThresholdSupportedChanged,
            this,
            &ProfilesConfigKCM::isChargeStopThresholdSupportedChanged);
    connect(m_externalServiceSettings,
            &ExternalServiceSettings::chargeStopThresholdMightNeedReconnectChanged,
            this,
            &ProfilesConfigKCM::chargeStopThresholdMightNeedReconnectChanged);

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &ProfilesConfigKCM::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &ProfilesConfigKCM::onServiceUnregistered);

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }

    // Load all the plugins, so we can tell the UI which ones are supported
    const QVector<KPluginMetaData> offers = KPluginMetaData::findPlugins(QStringLiteral("powerdevil/action"));

    for (const KPluginMetaData &offer : offers) {
        QString actionId = offer.value(QStringLiteral("X-KDE-PowerDevil-Action-ID"));
        bool supported = true;

        // Does it have a runtime requirement?
        if (offer.value(QStringLiteral("X-KDE-PowerDevil-Action-HasRuntimeRequirement"), false)) {
            qCDebug(POWERDEVIL) << offer.name() << " has a runtime requirement";

            QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement",
                                                               "/org/kde/Solid/PowerManagement",
                                                               "org.kde.Solid.PowerManagement",
                                                               "isActionSupported");
            call.setArguments(QVariantList() << actionId);
            QDBusPendingReply<bool> reply = QDBusConnection::sessionBus().asyncCall(call);
            reply.waitForFinished();

            if (reply.isValid()) {
                if (!reply.value()) {
                    supported = false;
                    qCDebug(POWERDEVIL) << "The action " << actionId << " appears not to be supported by the core.";
                }
            } else {
                qCDebug(POWERDEVIL) << "There was a problem in contacting DBus!! Assuming the action is ok.";
            }
        }

        m_supportedActions.insert(actionId, supported);
    }
}

void ProfilesConfigKCM::load()
{
    QWindow *renderWindowAsKAuthParent = QQuickRenderControl::renderWindowFor(mainUi()->window());
    m_externalServiceSettings->load(renderWindowAsKAuthParent);

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for (const Solid::Device &device : devices) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery *>(device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply()) {
            setPowerSupplyBatteryPresent(true);
            if (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery) {
                setSupportsBatteryProfiles(true);
            }
        } else {
            setPeripheralBatteryPresent(true);
        }
    }

    setLidPresent(LidController().isLidPresent());
    setPowerButtonPresent(true /* HACK This needs proper API to determine! */);

    KQuickManagedConfigModule::load();
}

void ProfilesConfigKCM::save()
{
    KQuickManagedConfigModule::save();

    QWindow *renderWindowAsKAuthParent = QQuickRenderControl::renderWindowFor(mainUi()->window());
    m_externalServiceSettings->save(renderWindowAsKAuthParent);

    // Notify daemon
    QDBusMessage call =
        QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement", "org.kde.Solid.PowerManagement", "refreshStatus");
    QDBusConnection::sessionBus().asyncCall(call);
}

bool ProfilesConfigKCM::isSaveNeeded() const
{
    return m_externalServiceSettings->isSaveNeeded();
}

QVariantMap ProfilesConfigKCM::supportedActions() const
{
    return m_supportedActions;
}

ProfilesConfigData *ProfilesConfigKCM::settings() const
{
    return m_settings;
}

ExternalServiceSettings *ProfilesConfigKCM::externalServiceSettings() const
{
    return m_externalServiceSettings;
}

QString ProfilesConfigKCM::currentProfile() const
{
    return m_currentProfile;
}

void ProfilesConfigKCM::setCurrentProfile(const QString &currentProfile)
{
    if (currentProfile == m_currentProfile) {
        return;
    }
    m_currentProfile = currentProfile;
    Q_EMIT currentProfileChanged();
}

bool ProfilesConfigKCM::supportsBatteryProfiles() const
{
    return m_supportsBatteryProfiles;
}

void ProfilesConfigKCM::setSupportsBatteryProfiles(bool supportsBatteryProfiles)
{
    if (supportsBatteryProfiles == m_supportsBatteryProfiles) {
        return;
    }
    m_supportsBatteryProfiles = supportsBatteryProfiles;
    Q_EMIT supportsBatteryProfilesChanged();
}

bool ProfilesConfigKCM::isChargeStartThresholdSupported() const
{
    return m_externalServiceSettings->isChargeStartThresholdSupported();
}

bool ProfilesConfigKCM::isChargeStopThresholdSupported() const
{
    return m_externalServiceSettings->isChargeStopThresholdSupported();
}

bool ProfilesConfigKCM::chargeStopThresholdMightNeedReconnect() const
{
    return m_externalServiceSettings->chargeStopThresholdMightNeedReconnect();
}

bool ProfilesConfigKCM::isPowerSupplyBatteryPresent() const
{
    return m_isPowerSupplyBatteryPresent;
}

bool ProfilesConfigKCM::isPeripheralBatteryPresent() const
{
    return m_isPeripheralBatteryPresent;
}

void ProfilesConfigKCM::setPowerSupplyBatteryPresent(bool isBatteryPresent)
{
    if (isBatteryPresent == m_isPowerSupplyBatteryPresent) {
        return;
    }
    m_isPowerSupplyBatteryPresent = isBatteryPresent;
    Q_EMIT isPowerSupplyBatteryPresentChanged();
}

void ProfilesConfigKCM::setPeripheralBatteryPresent(bool isBatteryPresent)
{
    if (isBatteryPresent == m_isPeripheralBatteryPresent) {
        return;
    }
    m_isPeripheralBatteryPresent = isBatteryPresent;
    Q_EMIT isPeripheralBatteryPresentChanged();
}

bool ProfilesConfigKCM::isLidPresent() const
{
    return m_isLidPresent;
}

bool ProfilesConfigKCM::isPowerButtonPresent() const
{
    return m_isPowerButtonPresent;
}

void ProfilesConfigKCM::setLidPresent(bool isLidPresent)
{
    if (isLidPresent == m_isLidPresent) {
        return;
    }
    m_isLidPresent = isLidPresent;
    Q_EMIT isLidPresentChanged();
}

void ProfilesConfigKCM::setPowerButtonPresent(bool isPowerButtonPresent)
{
    if (isPowerButtonPresent == m_isPowerButtonPresent) {
        return;
    }
    m_isPowerButtonPresent = isPowerButtonPresent;
    Q_EMIT isPowerButtonPresentChanged();
}

QObject *ProfilesConfigKCM::autoSuspendActionModel() const
{
    return m_autoSuspendActionModel;
}

QObject *ProfilesConfigKCM::batteryCriticalActionModel() const
{
    return m_batteryCriticalActionModel;
}

QObject *ProfilesConfigKCM::powerButtonActionModel() const
{
    return m_powerButtonActionModel;
}

QObject *ProfilesConfigKCM::lidActionModel() const
{
    return m_lidActionModel;
}

QObject *ProfilesConfigKCM::sleepModeModel() const
{
    return m_sleepModeModel;
}

QObject *ProfilesConfigKCM::powerProfileModel() const
{
    return m_powerProfileModel;
}

bool ProfilesConfigKCM::powerManagementServiceRegistered() const
{
    return m_powerManagementServiceRegistered;
}

QString ProfilesConfigKCM::powerManagementServiceErrorReason() const
{
    return m_powerManagementServiceErrorReason;
}

void ProfilesConfigKCM::setPowerManagementServiceRegistered(bool registered)
{
    if (registered == m_powerManagementServiceRegistered) {
        return;
    }
    m_powerManagementServiceRegistered = registered;
    Q_EMIT powerManagementServiceRegisteredChanged();
}

void ProfilesConfigKCM::setPowerManagementServiceErrorReason(const QString &reason)
{
    if (reason == m_powerManagementServiceErrorReason) {
        return;
    }
    m_powerManagementServiceErrorReason = reason;
    Q_EMIT powerManagementServiceErrorReasonChanged();
}

void ProfilesConfigKCM::onServiceRegistered(const QString & /*service*/)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                       QStringLiteral("/org/kde/Solid/PowerManagement"),
                                                       QStringLiteral("org.kde.Solid.PowerManagement"),
                                                       QStringLiteral("currentProfile"));
    QDBusPendingCallWatcher *currentProfileWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);

    QObject::connect(currentProfileWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QString> reply = *watcher;

        if (!reply.isError()) {
            setCurrentProfile(reply.value());
        }

        watcher->deleteLater();
    });

    setPowerManagementServiceRegistered(true);
}

void ProfilesConfigKCM::onServiceUnregistered(const QString & /*service*/)
{
    setPowerManagementServiceErrorReason(i18n("The Power Management Service appears not to be running."));
    setPowerManagementServiceRegistered(false);
}

} // namespace PowerDevil

#include "ProfilesConfigKCM.moc"

#include "moc_ProfilesConfigKCM.cpp"
