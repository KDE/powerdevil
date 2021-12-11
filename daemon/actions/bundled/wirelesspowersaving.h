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


#ifndef POWERDEVIL_BUNDLEDACTIONS_WIRELESSPOWERSAVING_H
#define POWERDEVIL_BUNDLEDACTIONS_WIRELESSPOWERSAVING_H

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

#include <BluezQt/Manager>

namespace PowerDevil {
namespace BundledActions {

class WirelessPowerSaving : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(WirelessPowerSaving)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.WirelessPowerSaving")

public:
    enum PowerSavingOption {
        NoAction = 0,
        TurnOff = 1,
        TurnOn = 2
    };

    explicit WirelessPowerSaving(QObject* parent);
    ~WirelessPowerSaving() override = default;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override;
    void triggerImpl(const QVariantMap& args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup& config) override;

public Q_SLOTS:
    // DBus export
    void setBluetoothEnabled(bool enabled);
    void setMobileBroadbandEnabled(bool enabled);
    void setWirelessEnabled(bool enabled);

private:
    BluezQt::Manager *const m_btManager;

    QString m_currentProfile;
    QString m_lastProfile;

    // Options for current profile
    PowerSavingOption m_currentProfileWifiOption;
    PowerSavingOption m_currentProfileWwanOption;
    PowerSavingOption m_currentProfileBtOption;

    // Options for previous profile
    PowerSavingOption m_lastProfileWifiOption;
    PowerSavingOption m_lastProfileWwanOption;
    PowerSavingOption m_lastProfileBtOption;

    // State of devices before we change that due to changed profile
    bool m_lastWifiState = false;
    bool m_lastWwanState = false;
    bool m_lastBtState = false;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_WIRELESSPOWERSAVING_H
