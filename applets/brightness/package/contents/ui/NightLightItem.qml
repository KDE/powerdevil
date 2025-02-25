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
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

import org.kde.plasma.private.brightnesscontrolplugin
import org.kde.plasma.workspace.dbus as DBus

PlasmaComponents3.ItemDelegate {
    id: root

    required property DBus.Properties nightLightControl

    background.visible: highlighted
    highlighted: activeFocus
    hoverEnabled: false

    Accessible.description: status.text
    KeyNavigation.tab: inhibitionSwitch.visible ? inhibitionSwitch : kcmButton

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
            Layout.alignment: Qt.AlignTop
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

                    enabled: false
                }

                PlasmaComponents3.Label {
                    id: currentTemp
                    visible: root.nightLightControl.available && root.nightLightControl.enabled && root.nightLightControl.running
                    text: i18nc("Placeholder is screen color temperature", "%1K", root.nightLightControl.currentTemperature)
                    textFormat: Text.PlainText

                    horizontalAlignment: Text.AlignRight
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Switch {
                    id: inhibitionSwitch
                    visible: root.nightLightControl.enabled
                    enabled: root.nightLightControl.togglable
                    checked: root.nightLightControl.inhibited
                    text: i18nc("@action:button Night Light", "Suspend")

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
                    onClicked: NightLightInhibitor.toggleInhibition()
                }

                PlasmaComponents3.Button {
                    id: kcmButton
                    visible: KConfig.KAuthorized.authorizeControlModule("kcm_nightlight")

                    icon.name: "configure"
                    text: root.nightLightControl.enabled ? i18n("Configure…") : i18n("Enable and Configure…")

                    Layout.alignment: Qt.AlignRight

                    KeyNavigation.up: root.KeyNavigation.up
                    KeyNavigation.backtab: inhibitionSwitch.visible ? inhibitionSwitch : root
                    KeyNavigation.left: inhibitionSwitch

                    Keys.onPressed: (event) => {
                        if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                            clicked();
                            event.accepted = true
                        }
                    }
                    onClicked: KCMLauncher.openSystemSettings("kcm_nightlight")
                }
            }

            RowLayout {
                visible: root.nightLightControl.running && root.nightLightControl.hasSwitchingTimes

                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Label {
                    id: transitionLabel
                    text: {
                        if (root.nightLightControl.daylight) {
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

                    enabled: false
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                PlasmaComponents3.Label {
                    id: transitionTime
                    text: {
                        if (root.nightLightControl.transitioning) {
                            return new Date(root.nightLightControl.currentTransitionEndTime).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                        } else {
                            return new Date(root.nightLightControl.scheduledTransitionStartTime).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                        }
                    }
                    textFormat: Text.PlainText

                    enabled: false
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }
}
