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

#include <NetworkManagerQt/Manager>

namespace PowerDevil {
namespace BundledActions {

WirelessPowerSaving::WirelessPowerSaving(QObject* parent)
    : Action(parent)
    , m_btManager(new BluezQt::Manager())
    , m_currentProfileWifiOption(PowerDevilEnums::WirelessMode::TurnOff)
    , m_currentProfileWwanOption(PowerDevilEnums::WirelessMode::TurnOff)
    , m_currentProfileBtOption(PowerDevilEnums::WirelessMode::TurnOff)
    , m_lastProfileWifiOption(PowerDevilEnums::WirelessMode::TurnOff)
    , m_lastProfileWwanOption(PowerDevilEnums::WirelessMode::TurnOff)
    , m_lastProfileBtOption(PowerDevilEnums::WirelessMode::TurnOff)
{
    // DBus
    new WirelessPowerSavingAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::None);
}

void WirelessPowerSaving::onProfileUnload()
{
    if (m_lastWifiState && m_currentProfileWifiOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setWirelessEnabled(true);
    } else if (!m_lastWifiState && m_currentProfileWifiOption == PowerDevilEnums::WirelessMode::TurnOn) {
        setWirelessEnabled(false);
    }

    if (m_lastWwanState && m_currentProfileWwanOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setMobileBroadbandEnabled(true);
    } else if (!m_lastWwanState && m_currentProfileWwanOption == PowerDevilEnums::WirelessMode::TurnOn) {
        setMobileBroadbandEnabled(false);
    }

    if (m_lastBtState && m_currentProfileBtOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setBluetoothEnabled(true);
    } else if (!m_lastBtState && m_currentProfileBtOption == PowerDevilEnums::WirelessMode::TurnOn) {
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
         (((m_lastProfileWifiOption == PowerDevilEnums::WirelessMode::TurnOff && m_currentProfileWifiOption == PowerDevilEnums::WirelessMode::TurnOn) || m_currentProfileWifiOption == PowerDevilEnums::WirelessMode::NoAction) &&
          ((m_lastProfileWwanOption == PowerDevilEnums::WirelessMode::TurnOff && m_currentProfileWwanOption == PowerDevilEnums::WirelessMode::TurnOn) || m_currentProfileWwanOption == PowerDevilEnums::WirelessMode::NoAction) &&
          ((m_lastProfileBtOption == PowerDevilEnums::WirelessMode::TurnOff && m_currentProfileBtOption == PowerDevilEnums::WirelessMode::TurnOn) || m_currentProfileBtOption == PowerDevilEnums::WirelessMode::NoAction))) {
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
    const auto wifiOption = static_cast<PowerDevilEnums::WirelessMode>(args.value(QLatin1String("wifiOption")).toInt());
    const auto wwanOption = static_cast<PowerDevilEnums::WirelessMode>(args.value(QLatin1String("wwanOption")).toInt());
    const auto btOption = static_cast<PowerDevilEnums::WirelessMode>(args.value(QLatin1String("btOption")).toInt());

    if (wifiOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setWirelessEnabled(false);
    } else if (wifiOption == PowerDevilEnums::WirelessMode::TurnOn) {
        setWirelessEnabled(true);
    }

    if (wwanOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setMobileBroadbandEnabled(false);
    } else if (wwanOption == PowerDevilEnums::WirelessMode::TurnOn) {
        setMobileBroadbandEnabled(true);
    }

    if (btOption == PowerDevilEnums::WirelessMode::TurnOff) {
        setBluetoothEnabled(false);
    } else if (btOption == PowerDevilEnums::WirelessMode::TurnOn) {
        setBluetoothEnabled(true);
    }
}

bool WirelessPowerSaving::isSupported()
{
    /* Check if we can enable/disable bluetooth ???
     * This would require to check if we can read/write from/to /dev/rfkill. Maybe Rfkill class from bluez-qt
     * can be used for this, but it seems to be private class
     */

    NMStringMap permissions = NetworkManager::permissions();
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

bool WirelessPowerSaving::loadAction(const PowerDevilProfileSettings &settings)
{
    // Handle profile changes
    m_lastProfile = m_currentProfile;
    m_currentProfile = settings.profileName();

    qCDebug(POWERDEVIL) << "Profiles: " << m_currentProfile << m_lastProfile;

    m_lastProfileWifiOption = m_currentProfileWifiOption;
    m_lastProfileWwanOption = m_currentProfileWwanOption;
    m_lastProfileBtOption = m_currentProfileBtOption;

    m_currentProfileWifiOption = static_cast<PowerDevilEnums::WirelessMode>(settings.wifiPowerSaving());
    m_currentProfileWwanOption = static_cast<PowerDevilEnums::WirelessMode>(settings.mobileBroadbandPowerSaving());
    m_currentProfileBtOption = static_cast<PowerDevilEnums::WirelessMode>(settings.bluetoothPowerSaving());

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
