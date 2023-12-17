/*
 *  SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ExternalServiceSettings.h"

// KDE
#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KLocalizedString>
#include <Solid/Battery>
#include <Solid/Device>

// Qt
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QPointer>

namespace
{
constexpr int ChargeThresholdUnsupported = -1;
}

namespace PowerDevil
{

ExternalServiceSettings::ExternalServiceSettings(QObject *parent)
    : QObject(parent)
    , m_chargeStartThreshold(ChargeThresholdUnsupported)
    , m_chargeStopThreshold(ChargeThresholdUnsupported)
    , m_savedChargeStartThreshold(ChargeThresholdUnsupported)
    , m_savedChargeStopThreshold(ChargeThresholdUnsupported)
    , m_chargeStopThresholdMightNeedReconnect(false)
{
}

void ExternalServiceSettings::load(QWindow *parentWindowForKAuth)
{
    KAuth::Action action(QStringLiteral("org.kde.powerdevil.chargethresholdhelper.getthreshold"));
    action.setHelperId(QStringLiteral("org.kde.powerdevil.chargethresholdhelper"));
    action.setParentWindow(parentWindowForKAuth);
    KAuth::ExecuteJob *job = action.execute();

    QPointer thisAlive(this);
    QPointer jobAlive(job);
    job->exec();

    if (thisAlive && jobAlive) {
        if (!job->error()) {
            const auto data = job->data();
            setSavedChargeStartThreshold(data.value(QStringLiteral("chargeStartThreshold")).toInt());
            setSavedChargeStopThreshold(data.value(QStringLiteral("chargeStopThreshold")).toInt());
            setChargeStopThreshold(m_savedChargeStopThreshold);
            setChargeStartThreshold(m_savedChargeStartThreshold);
        } else {
            qWarning() << "org.kde.powerdevil.chargethresholdhelper.getthreshold failed:" << job->errorText();
            setSavedChargeStartThreshold(ChargeThresholdUnsupported);
            setSavedChargeStopThreshold(ChargeThresholdUnsupported);
        }
    } else {
        qWarning() << "org.kde.powerdevil.chargethresholdhelper.getthreshold failed: was deleted during job execution";
    }
}

void ExternalServiceSettings::save(QWindow *parentWindowForKAuth)
{
    if ((isChargeStartThresholdSupported() && m_chargeStartThreshold != m_savedChargeStartThreshold)
        || (isChargeStopThresholdSupported() && m_chargeStopThreshold != m_savedChargeStopThreshold)) {
        int newChargeStartThreshold = isChargeStartThresholdSupported() ? m_chargeStartThreshold : ChargeThresholdUnsupported;
        int newChargeStopThreshold = isChargeStopThresholdSupported() ? m_chargeStopThreshold : ChargeThresholdUnsupported;

        KAuth::Action action(QStringLiteral("org.kde.powerdevil.chargethresholdhelper.setthreshold"));
        action.setHelperId(QStringLiteral("org.kde.powerdevil.chargethresholdhelper"));
        action.setArguments({
            {QStringLiteral("chargeStartThreshold"), newChargeStartThreshold},
            {QStringLiteral("chargeStopThreshold"), newChargeStopThreshold},
        });
        action.setParentWindow(parentWindowForKAuth);
        KAuth::ExecuteJob *job = action.execute();

        QPointer thisAlive(this);
        QPointer jobAlive(job);
        job->exec();

        if (thisAlive && jobAlive) {
            if (!job->error()) {
                setSavedChargeStartThreshold(newChargeStartThreshold);
                setSavedChargeStopThreshold(newChargeStopThreshold);
            } else {
                qWarning() << "org.kde.powerdevil.chargethresholdhelper.setthreshold failed:" << job->errorText();
            }
            setChargeStopThreshold(m_savedChargeStopThreshold);
            setChargeStartThreshold(m_savedChargeStartThreshold);
        } else {
            qWarning() << "org.kde.powerdevil.chargethresholdhelper.setthreshold failed: was deleted during job execution";
        }
    }
}

bool ExternalServiceSettings::isSaveNeeded() const
{
    return (isChargeStartThresholdSupported() && m_chargeStartThreshold != m_savedChargeStartThreshold)
        || (isChargeStopThresholdSupported() && m_chargeStopThreshold != m_savedChargeStopThreshold);
}

bool ExternalServiceSettings::isChargeStartThresholdSupported() const
{
    return m_savedChargeStartThreshold != ChargeThresholdUnsupported;
}

bool ExternalServiceSettings::isChargeStopThresholdSupported() const
{
    return m_savedChargeStopThreshold != ChargeThresholdUnsupported;
}

void ExternalServiceSettings::setSavedChargeStartThreshold(int threshold)
{
    bool wasChargeStartThresholdSupported = isChargeStartThresholdSupported();
    m_savedChargeStartThreshold = threshold;
    if (wasChargeStartThresholdSupported != isChargeStartThresholdSupported()) {
        Q_EMIT isChargeStartThresholdSupportedChanged();
    }
}

void ExternalServiceSettings::setSavedChargeStopThreshold(int threshold)
{
    bool wasChargeStopThresholdSupported = isChargeStopThresholdSupported();
    m_savedChargeStopThreshold = threshold;
    if (wasChargeStopThresholdSupported != isChargeStopThresholdSupported()) {
        Q_EMIT isChargeStopThresholdSupportedChanged();
    }
}

int ExternalServiceSettings::chargeStartThreshold() const
{
    return m_chargeStartThreshold;
}

int ExternalServiceSettings::chargeStopThreshold() const
{
    return m_chargeStopThreshold;
}

void ExternalServiceSettings::setChargeStartThreshold(int threshold)
{
    if (threshold == m_chargeStartThreshold) {
        return;
    }
    m_chargeStartThreshold = threshold;
    Q_EMIT chargeStartThresholdChanged();
    Q_EMIT settingsChanged();
}

void ExternalServiceSettings::setChargeStopThreshold(int threshold)
{
    if (threshold == m_chargeStopThreshold) {
        return;
    }
    m_chargeStopThreshold = threshold;
    Q_EMIT chargeStopThresholdChanged();

    if (m_chargeStopThreshold > m_savedChargeStopThreshold) {
        // Only show message if there is actually a charging or fully charged battery
        const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
        for (const Solid::Device &device : devices) {
            const Solid::Battery *b = qobject_cast<const Solid::Battery *>(device.asDeviceInterface(Solid::DeviceInterface::Battery));
            if (b->chargeState() == Solid::Battery::Charging || b->chargeState() == Solid::Battery::FullyCharged) {
                setChargeStopThresholdMightNeedReconnect(true);
                break;
            }
        }
    } else {
        setChargeStopThresholdMightNeedReconnect(false);
    }

    Q_EMIT settingsChanged();
}

bool ExternalServiceSettings::chargeStopThresholdMightNeedReconnect() const
{
    return m_chargeStopThresholdMightNeedReconnect;
}

void ExternalServiceSettings::setChargeStopThresholdMightNeedReconnect(bool mightNeedReconnect)
{
    if (mightNeedReconnect == m_chargeStopThresholdMightNeedReconnect) {
        return;
    }
    m_chargeStopThresholdMightNeedReconnect = mightNeedReconnect;
    Q_EMIT chargeStopThresholdMightNeedReconnectChanged();
}

} // namespace PowerDevil

#include "moc_ExternalServiceSettings.cpp"
