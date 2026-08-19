/*
    SPDX-FileCopyrightText: 2026 Matthias Kurz <m.kurz@irregular.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtTest

import "../applets/batterymonitor" as BatteryMonitor
import org.kde.plasma.private.battery

Item {
    id: testRoot

    width: 800
    height: 300

    Component {
        id: batteryItemComponent

        BatteryMonitor.BatteryItem {
            width: 700
            height: 200
            batteryPluggedIn: true
            batteryPrettyName: "Test battery"
            batteryType: "Headset"
            batteryChargeState: BatteryControlModel.Discharging
        }
    }

    TestCase {
        name: "BatteryItem"
        when: windowShown

        function test_knownPercentage() {
            const item = createTemporaryObject(batteryItemComponent, testRoot, { batteryPercent: 42 });
            verify(item);

            const percentageLabel = findChild(item, "percentageLabel");
            verify(percentageLabel);
            compare(percentageLabel.text, "42%");

            const statusLabel = findChild(item, "statusLabel");
            verify(statusLabel);
            compare(statusLabel.text, "Discharging");
            compare(statusLabel.visible, true);

            const chargeBar = findChild(item, "chargeBar");
            verify(chargeBar);
            compare(chargeBar.visible, true);
            compare(chargeBar.value, 42);
        }

        function test_unknownPercentage() {
            const item = createTemporaryObject(batteryItemComponent, testRoot, { batteryPercent: -1 });
            verify(item);

            const percentageLabel = findChild(item, "percentageLabel");
            verify(percentageLabel);
            compare(percentageLabel.text, "Unknown");

            const statusLabel = findChild(item, "statusLabel");
            verify(statusLabel);
            compare(statusLabel.visible, false);

            const chargeBar = findChild(item, "chargeBar");
            verify(chargeBar);
            compare(chargeBar.visible, false);
        }

        function test_unknownPercentageWhileCharging() {
            const item = createTemporaryObject(batteryItemComponent, testRoot, {
                batteryPercent: -1,
                batteryChargeState: BatteryControlModel.Charging
            });
            verify(item);

            const statusLabel = findChild(item, "statusLabel");
            verify(statusLabel);
            compare(statusLabel.text, "Charging");
            compare(statusLabel.visible, true);
        }
    }
}
