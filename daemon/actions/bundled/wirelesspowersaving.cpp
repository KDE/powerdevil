/***************************************************************************
 *   Copyright (C) 2016 by Jan Grulich <jgrulich@redhat.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "wirelesspowersaving.h"

#include "wirelesspowersavingadaptor.h"

#include <powerdevilcore.h>
#include <powerdevil_debug.h>

#include <KConfigGroup>
#include <KPluginFactory>

#include <NetworkManagerQt/Manager>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::WirelessPowerSaving, "powerdevilwirelesspowersavingaction.json")

namespace PowerDevil {
namespace BundledActions {
WirelessPowerSaving::WirelessPowerSaving(QObject *parent, const QVariantList &)
    : Action(parent)
    , m_btManager(new BluezQt::Manager())
    , m_currentProfileWifiOption(BundledActions::WirelessPowerSaving::TurnOff)
    , m_currentProfileWwanOption(BundledActions::WirelessPowerSaving::TurnOff)
    , m_currentProfileBtOption(BundledActions::WirelessPowerSaving::TurnOff)
    , m_lastProfileWifiOption(BundledActions::WirelessPowerSaving::TurnOff)
    , m_lastProfileWwanOption(BundledActions::WirelessPowerSaving::TurnOff)
    , m_lastProfileBtOption(BundledActions::WirelessPowerSaving::TurnOff)
{
    // DBus
    new WirelessPowerSavingAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::None);
}

void WirelessPowerSaving::onProfileUnload()
{
    if (m_lastWifiState && m_currentProfileWifiOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setWirelessEnabled(true);
    } else if (!m_lastWifiState && m_currentProfileWifiOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setWirelessEnabled(false);
    }

    if (m_lastWwanState && m_currentProfileWwanOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setMobileBroadbandEnabled(true);
    } else if (!m_lastWwanState && m_currentProfileWwanOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setMobileBroadbandEnabled(false);
    }

    if (m_lastBtState && m_currentProfileBtOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setBluetoothEnabled(true);
    } else if (!m_lastBtState && m_currentProfileBtOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setBluetoothEnabled(false);
    }
}

void WirelessPowerSaving::onWakeupFromIdle()
{
    // Nothing to do
}

void WirelessPowerSaving::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
    // Nothing to do
}

void WirelessPowerSaving::onProfileLoad()
{
    qCDebug(POWERDEVIL) << m_currentProfile << m_lastProfile;

    if (((m_currentProfile == QLatin1String("Battery") && m_lastProfile == QLatin1String("AC")) ||
         (m_currentProfile == QLatin1String("LowBattery") && (m_lastProfile == QLatin1String("AC") || m_lastProfile == QLatin1String("Battery")))) &&
         (((m_lastProfileWifiOption == BundledActions::WirelessPowerSaving::TurnOff && m_currentProfileWifiOption == BundledActions::WirelessPowerSaving::TurnOn) || m_currentProfileWifiOption == NoAction) &&
          ((m_lastProfileWwanOption == BundledActions::WirelessPowerSaving::TurnOff && m_currentProfileWwanOption == BundledActions::WirelessPowerSaving::TurnOn) || m_currentProfileWwanOption == NoAction) &&
          ((m_lastProfileBtOption == BundledActions::WirelessPowerSaving::TurnOff && m_currentProfileBtOption == BundledActions::WirelessPowerSaving::TurnOn) || m_currentProfileBtOption == NoAction))) {
        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing anything, the current profile is more conservative";
    } else {
        QVariantMap args{
            {{QLatin1String("wifiOption"), QVariant::fromValue((uint)m_currentProfileWifiOption)},
             {QLatin1String("wwanOption"), QVariant::fromValue((uint)m_currentProfileWwanOption)},
             {QLatin1String("btOption"), QVariant::fromValue((uint)m_currentProfileBtOption)}}
        };

        trigger(args);
    }
}

void WirelessPowerSaving::triggerImpl(const QVariantMap& args)
{
    const PowerSavingOption wifiOption = (PowerSavingOption)args.value(QLatin1String("wifiOption")).toUInt();
    const PowerSavingOption wwanOption = (PowerSavingOption)args.value(QLatin1String("wwanOption")).toUInt();
    const PowerSavingOption btOption = (PowerSavingOption)args.value(QLatin1String("btOption")).toUInt();

    if (wifiOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setWirelessEnabled(false);
    } else if (wifiOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setWirelessEnabled(true);
    }

    if (wwanOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setMobileBroadbandEnabled(false);
    } else if (wwanOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setMobileBroadbandEnabled(true);
    }

    if (btOption == BundledActions::WirelessPowerSaving::TurnOff) {
        setBluetoothEnabled(false);
    } else if (btOption == BundledActions::WirelessPowerSaving::TurnOn) {
        setBluetoothEnabled(true);
    }
}

bool WirelessPowerSaving::isSupported()
{
    /* Check if we can enable/disable bluetooth ???
     * This would require to check if we can read/write from/to /dev/rfkill. Maybe Rfkill class from bluez-qt
     * can be used for this, but it seems to be private class
     */

    static NMStringMap permissions = NetworkManager::permissions();
    bool changeWifiAllowed = false;
    bool changeWwanAllowed = false;
    for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
        if (it.key() == QLatin1String("org.freedesktop.NetworkManager.enable-disable-wifi")) {
            changeWifiAllowed = it.value() == QLatin1String("yes");
        } else if (it.key() == QLatin1String("org.freedesktop.NetworkManager.enable-disable-wwan")) {
            changeWwanAllowed = it.value() == QLatin1String("yes");
        }
    }

    return changeWifiAllowed || changeWwanAllowed;
}

bool WirelessPowerSaving::loadAction(const KConfigGroup& config)
{
    // Handle profile changes
    m_lastProfile = m_currentProfile;
    m_currentProfile = config.parent().name();

    qCDebug(POWERDEVIL) << "Profiles: " << m_currentProfile << m_lastProfile;

    m_lastProfileWifiOption = m_currentProfileWifiOption;
    m_lastProfileWwanOption = m_currentProfileWwanOption;
    m_lastProfileBtOption = m_currentProfileBtOption;

    if (config.hasKey("wifiOption")) {
        m_currentProfileWifiOption = (PowerSavingOption)config.readEntry<uint>("wifiOption", 0);
    }

    if (config.hasKey("wwanOption")) {
        m_currentProfileWwanOption = (PowerSavingOption)config.readEntry<uint>("wwanOption", 0);
    }

    if (config.hasKey("btOption")) {
        m_currentProfileBtOption = (PowerSavingOption)config.readEntry<uint>("btOption", 0);
    }


    m_lastWifiState = NetworkManager::isWirelessEnabled();
    m_lastWwanState = NetworkManager::isWwanEnabled();
    m_lastBtState = !m_btManager->isBluetoothBlocked();

    return true;
}

void WirelessPowerSaving::setBluetoothEnabled(bool enabled)
{
    m_btManager->setBluetoothBlocked(!enabled);
}

void WirelessPowerSaving::setMobileBroadbandEnabled(bool enabled)
{
    NetworkManager::setWwanEnabled(enabled);
}

void WirelessPowerSaving::setWirelessEnabled(bool enabled)
{
    NetworkManager::setWirelessEnabled(enabled);
}

}

}
#include "wirelesspowersaving.moc"
