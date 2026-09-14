/*
 * SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 * SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
 * SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie.clarius@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import org.kde.kcmutils // KCMLauncher
import org.kde.config as KConfig  // KAuthorized.authorizeControlModule
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

import org.kde.plasma.private.brightnesscontrolplugin
import org.kde.plasma.workspace.dbus as DBus

PlasmaComponents3.ItemDelegate {
    id: root

    required property NightLightControl nightLightControl

    background.visible: highlighted
    highlighted: activeFocus
    hoverEnabled: false

    Accessible.description: status.text
    KeyNavigation.tab: quickToggle

    DayNightSchedule {
        id: dayNightSchedule
    }

    component NightLightControl: DBus.Properties {
        busType: DBus.BusType.Session
        service: "org.kde.KWin.NightLight"
        path: "/org/kde/KWin/NightLight"
        iface: "org.kde.KWin.NightLight"

        // This property holds a value to indicate if Night Light is available.
        readonly property bool available: Boolean(properties.available)
        // This property holds a value to indicate if Night Light is enabled.
        readonly property bool enabled: Boolean(properties.enabled)
        // This property holds a value to indicate if Night Light is running.
        readonly property bool running: Boolean(properties.running)
        // This property holds a value to indicate whether night light is currently inhibited.
        readonly property bool inhibited: Boolean(properties.inhibited)
        // This property holds a value to indicate which mode is set for transitions (0 - automatic location, 1 - manual location, 2 - manual timings, 3 - constant)
        readonly property int mode: Number(properties.mode)
        // This property holds a value to indicate if Night Light is on day mode.
        readonly property bool daylight: Boolean(properties.daylight)
        // This property holds a value to indicate currently applied color temperature.
        readonly property int currentTemperature: Number(properties.currentTemperature)
        // This property holds a value to indicate currently applied color temperature.
        readonly property int targetTemperature: Number(properties.targetTemperature)
        // This property holds a value to indicate the end time of the previous color transition in msec since epoch.
        readonly property double currentTransitionEndTime: Number(properties.previousTransitionDateTime) * 1000 + Number(properties.previousTransitionDuration)
        // This property holds a value to indicate the start time of the next color transition in msec since epoch.
        readonly property double scheduledTransitionStartTime: Number(properties.scheduledTransitionDateTime) * 1000
        // This property holds a value to indicate the date and time until which Night Light is temporarily activated.
        readonly property double activatedUntil: Number(properties.activatedUntil) * 1000
        // This property holds a value to indicate the date and time until which Night Light is temporarily deactivated.
        readonly property double deactivatedUntil: Number(properties.deactivatedUntil) * 1000

        readonly property bool transitioning: currentTemperature != targetTemperature
        readonly property bool hasSwitchingTimes: mode != 0

        function activateUntil(timestamp) {
            DBus.SessionBus.asyncCall({
                service: "org.kde.KWin.NightLight",
                path: "/org/kde/KWin/NightLight",
                iface: "org.kde.KWin.NightLight",
                member: "activateUntil",
                arguments: [timestamp / 1000],
                signature: "(t)",
            });
        }

        function deactivateUntil(timestamp) {
            DBus.SessionBus.asyncCall({
                service: "org.kde.KWin.NightLight",
                path: "/org/kde/KWin/NightLight",
                iface: "org.kde.KWin.NightLight",
                member: "deactivateUntil",
                arguments: [timestamp / 1000],
                signature: "(t)",
            });
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.gridUnit

        Kirigami.Icon {
            id: image
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: {
                if (!root.nightLightControl.enabled) {
                    return "redshift-status-on"; // not configured: show generic night light icon rather "manually turned off" icon
                } else if (!root.nightLightControl.running) {
                    return "redshift-status-off";
                } else if (root.nightLightControl.daylight && root.nightLightControl.targetTemperature != 6500) { // show daylight icon only when temperature during the day is actually modified
                    return "redshift-status-day";
                } else {
                    return "redshift-status-on";
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Label {
                    id: title
                    text: root.text
                    textFormat: Text.PlainText

                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                PlasmaComponents3.Label {
                    id: status
                    text: {
                        if (root.nightLightControl.inhibited && root.nightLightControl.enabled) {
                            return i18nc("Night light status", "Suspended");
                        }
                        if (!root.nightLightControl.available) {
                            return i18nc("Night light status", "Unavailable");
                        }
                        if (root.nightLightControl.activatedUntil) {
                            return i18nc("Night light status", "On");
                        }
                        if (root.nightLightControl.deactivatedUntil) {
                            return i18nc("Night light status", "Paused");
                        }
                        if (!root.nightLightControl.enabled) {
                            return i18nc("Night light status", "Not enabled");
                        }
                        if (!root.nightLightControl.running) {
                            return i18nc("Night light status", "Not running");
                        }
                        if (!root.nightLightControl.hasSwitchingTimes) {
                            return i18nc("Night light status", "On");
                        }
                        if (root.nightLightControl.daylight && root.nightLightControl.transitioning) {
                            return i18nc("Night light phase", "Morning Transition");
                        } else if (root.nightLightControl.daylight) {
                            return i18nc("Night light phase", "Day");
                        } else if (root.nightLightControl.transitioning) {
                            return i18nc("Night light phase", "Evening Transition");
                        } else {
                            return i18nc("Night light phase", "Night");
                        }
                    }
                    textFormat: Text.PlainText

                    opacity: 0.75
                }

                PlasmaComponents3.Label {
                    id: currentTemp
                    visible: root.nightLightControl.available && root.nightLightControl.enabled && root.nightLightControl.running
                    text: i18nc("Placeholder is screen color temperature", "%1K", root.nightLightControl.currentTemperature)
                    font.features: { "tnum": 1 }
                    textFormat: Text.PlainText

                    horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Switch {
                    id: quickToggle
                    enabled: !root.nightLightControl.inhibited
                    checked: root.nightLightControl.running || root.nightLightControl.activatedUntil || root.nightLightControl.deactivatedUntil
                    text: i18nc("@action:button Night Light", "Toggle")

                    Layout.fillWidth: true

                    Accessible.onPressAction: clicked()

                    KeyNavigation.up: root.KeyNavigation.up
                    KeyNavigation.tab: kcmButton
                    KeyNavigation.right: kcmButton
                    KeyNavigation.backtab: root

                    Keys.onPressed: (event) => {
                        if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                            clicked()
                            event.accepted = true
                        }
                    }
                    onClicked: {
                        if (root.nightLightControl.activatedUntil) {
                            root.nightLightControl.activateUntil(0);
                            return;
                        }

                        if (root.nightLightControl.deactivatedUntil) {
                            root.nightLightControl.deactivateUntil(0);
                            return;
                        }

                        if (root.nightLightControl.daylight) {
                            root.nightLightControl.activateUntil(dayNightSchedule.nextMorning.startDateTime);
                        } else {
                            root.nightLightControl.deactivateUntil(dayNightSchedule.nextEvening.startDateTime);
                        }
                    }
                }

                PlasmaComponents3.Button {
                    id: kcmButton
                    visible: KConfig.KAuthorized.authorizeControlModule("kcm_nightlight")

                    icon.name: "configure"
                    text: i18n("Configure…")

                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    KeyNavigation.up: root.KeyNavigation.up
                    KeyNavigation.backtab: quickToggle
                    KeyNavigation.left: quickToggle

                    Keys.onPressed: (event) => {
                        if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                            clicked();
                            event.accepted = true
                        }
                    }
                    onClicked: KCMLauncher.openSystemSettings("kcm_nightlight")
                }
            }

            PlasmaComponents3.Label {
                text: {
                    if (root.nightLightControl.daylight) {
                        const date = new Date(dayNightSchedule.nextMorning.startDateTime);
                        return i18nc("The placeholder indicates the time", "Temporarily activate until %1", date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }));
                    } else {
                        const date = new Date(dayNightSchedule.nextEvening.startDateTime);
                        return i18nc("The placeholder indicates the time", "Temporarily deactivate until %1", date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }));
                    }
                }
                textFormat: Text.PlainText

                visible: !root.nightLightControl.activatedUntil && !root.nightLightControl.deactivatedUntil
                opacity: 0.75
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                visible: root.nightLightControl.running && (root.nightLightControl.hasSwitchingTimes || root.nightLightControl.activatedUntil || root.nightLightControl.deactivatedUntil)

                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Label {
                    text: {
                        if (root.nightLightControl.activatedUntil) {
                            return i18nc("Label for a time", "Temporarily activated until:");
                        } else if (root.nightLightControl.deactivatedUntil) {
                            return i18nc("Label for a time", "Temporarily deactivated until:");
                        } else if (root.nightLightControl.daylight) {
                            if (root.nightLightControl.transitioning) {
                                return i18nc("Label for a time", "Transition to day complete by:");
                            }
                            return i18nc("Label for a time", "Transition to night scheduled for:");
                        } else if (root.nightLightControl.transitioning) {
                            return i18nc("Label for a time", "Transition to night complete by:");
                        } else {
                            return i18nc("Label for a time", "Transition to day scheduled for:");
                        }
                    }
                    textFormat: Text.PlainText

                    opacity: 0.75
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                PlasmaComponents3.Label {
                    text: {
                        let dateTime;

                        if (root.nightLightControl.activatedUntil) {
                            dateTime = new Date(root.nightLightControl.activatedUntil);
                        } else if (root.nightLightControl.deactivatedUntil) {
                            dateTime = new Date(root.nightLightControl.deactivatedUntil);
                        } else if (root.nightLightControl.transitioning) {
                            dateTime = new Date(root.nightLightControl.currentTransitionEndTime);
                        } else {
                            dateTime = new Date(root.nightLightControl.scheduledTransitionStartTime);
                        }

                        return dateTime.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                    }
                    textFormat: Text.PlainText

                    opacity: 0.75
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }
}
