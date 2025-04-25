// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@kde.org>

#include "mobilepower.h"
#include "statisticsprovider.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <Kirigami/Platform/TabletModeWatcher>

#include <Solid/Battery>
#include <QDBusConnection>
#include <QDBusPendingCall>

#include <powerdevilenums.h>
#include <powerdevilpowermanagement.h>

K_PLUGIN_CLASS_WITH_JSON(MobilePower, "kcm_mobile_power.json")

enum {
    THIRTY_SECONDS,
    ONE_MINUTE,
    TWO_MINUTES,
    FIVE_MINUTES,
    TEN_MINUTES,
    FIFTEEN_MINUTES,
    THIRTY_MINUTES,
    NEVER,
};

const QStringList timeValues = {
    i18n("30 sec"),
    i18n("1 min"),
    i18n("2 min"),
    i18n("5 min"),
    i18n("10 min"),
    i18n("15 min"),
    i18n("30 min"),
    i18n("Never"),
};

// Maps the indices of the timeValues indexes
// to seconds.
const QMap<int, qreal> idxToSeconds = {
    {THIRTY_SECONDS, 30},
    {ONE_MINUTE, 60},
    {TWO_MINUTES, 120},
    {FIVE_MINUTES, 300},
    {TEN_MINUTES, 600},
    {FIFTEEN_MINUTES, 900},
    {THIRTY_MINUTES, 1800},
    {NEVER, 0},
};

MobilePower::MobilePower(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_batteries{new BatteryModel(this)}
{
    qmlRegisterUncreatableType<BatteryModel>("org.kde.kcm.power.mobile.private", 1, 0, "BatteryModel", QStringLiteral("Use BatteryModel"));
    qmlRegisterUncreatableType<Solid::Battery>("org.kde.kcm.power.mobile.private", 1, 0, "Battery", QStringLiteral(""));
    qmlRegisterType<StatisticsProvider>("org.kde.kcm.power.mobile.private", 1, 0, "HistoryModel");

    setButtons(KQuickConfigModule::NoAdditionalButton);

    bool isMobile = Kirigami::Platform::TabletModeWatcher::self()->isTabletMode();
    bool isVM = PowerDevil::PowerManagement::instance()->isVirtualMachine();
    bool canSuspend = PowerDevil::PowerManagement::instance()->canSuspend();

    m_settingsAC = new PowerDevil::ProfileSettings(QStringLiteral("AC"), isMobile, isVM, canSuspend, this);
    m_settingsBattery = new PowerDevil::ProfileSettings(QStringLiteral("Battery"), isMobile, isVM, canSuspend, this);
    m_settingsLowBattery = new PowerDevil::ProfileSettings(QStringLiteral("LowBattery"), isMobile, isVM, canSuspend, this);
    m_settings = {m_settingsAC, m_settingsBattery, m_settingsLowBattery};

    load();
}

void MobilePower::load()
{
    // we assume that the [AC], [Battery], and [LowBattery] groups have the same value
    // (which is done by this kcm)

    m_dimScreenTime = m_settingsAC->dimDisplayIdleTimeoutSec();
    m_dimScreen = m_settingsAC->dimDisplayWhenIdle();

    m_screenOffTime = m_settingsAC->turnOffDisplayIdleTimeoutSec();
    m_screenOff = m_settingsAC->turnOffDisplayWhenIdle();

    m_suspendSessionTime = m_settingsAC->autoSuspendIdleTimeoutSec();
}

void MobilePower::save()
{
    // we set all profiles at the same time, since our UI is a simple global toggle
    for (auto *settings : m_settings) {
        settings->setDimDisplayIdleTimeoutSec(m_dimScreenTime);
        settings->setDimDisplayWhenIdle(m_dimScreen);
        settings->setTurnOffDisplayWhenIdle(m_screenOff);
        settings->setTurnOffDisplayIdleTimeoutSec(m_screenOffTime);
        settings->setAutoSuspendIdleTimeoutSec(m_suspendSessionTime);

        settings->save();
    }

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                       QStringLiteral("/org/kde/Solid/PowerManagement"),
                                                       QStringLiteral("org.kde.Solid.PowerManagement"),
                                                       QStringLiteral("refreshStatus"));
    QDBusConnection::sessionBus().asyncCall(call);
}

QStringList MobilePower::timeOptions() const
{
    return timeValues;
}

void MobilePower::setDimScreenIdx(int idx)
{
    qreal value = idxToSeconds.value(idx);
    qDebug() << "Got the value" << value;

    if (m_dimScreenTime == value) {
        return;
    }

    if (value == 0) {
        qDebug() << "Setting to never dim";
        m_dimScreen = false;
    } else {
        qDebug() << "Setting to dim in " << value << "Minutes";
        m_dimScreen = true;
    }

    m_dimScreenTime = value;
    Q_EMIT dimScreenIdxChanged();
    save();
}

void MobilePower::setScreenOffIdx(int idx)
{
    qreal value = idxToSeconds.value(idx);
    qDebug() << "Got the value" << value;

    if (m_screenOffTime == value) {
        return;
    }

    if (value == 0) {
        qDebug() << "Setting to never screen off";
        m_screenOff = false;
    } else {
        qDebug() << "Setting to screen off in " << value << "Minutes";
        m_screenOff = true;
    }
    m_screenOffTime = value;

    Q_EMIT screenOffIdxChanged();
    save();
}

void MobilePower::setSuspendSessionIdx(int idx)
{
    qreal value = idxToSeconds.value(idx);
    qDebug() << "Got the value" << value;

    if (m_suspendSessionTime == value) {
        return;
    }

    if (value == 0) {
        qDebug() << "Setting to never suspend";
    } else {
        qDebug() << "Setting to suspend in " << value << "Minutes";
    }

    m_suspendSessionTime = value;
    Q_EMIT suspendSessionIdxChanged();
    save();
}

int MobilePower::suspendSessionIdx()
{
    if (m_suspendSessionTime == 0) {
        return NEVER;
    }

    return idxToSeconds.key(std::round(m_suspendSessionTime));
}

int MobilePower::dimScreenIdx()
{
    if (!m_dimScreen) {
        return NEVER;
    }

    return idxToSeconds.key(std::round(m_dimScreenTime));
}

int MobilePower::screenOffIdx()
{
    if (!m_screenOff) {
        return NEVER;
    }

    return idxToSeconds.key(std::round(m_screenOffTime));
}

BatteryModel *MobilePower::batteries()
{
    return m_batteries;
}

#include "mobilepower.moc"
