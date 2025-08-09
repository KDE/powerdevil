/*
 * SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>
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

PlasmaComponents3.ItemDelegate {
    id: root

    background.visible: highlighted
    highlighted: activeFocus
    hoverEnabled: false

    Accessible.description: status.text
    KeyNavigation.tab: darkModeSwitch

    contentItem: RowLayout {
        spacing: Kirigami.Units.gridUnit

        Kirigami.Icon {
            id: image
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: "lighttable"
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
                    text: DarkModeControl.currentTheme
                    textFormat: Text.PlainText

                    opacity: 0.75
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Switch {
                    id: darkModeSwitch
                    Layout.fillWidth: true
                    text: i18nc("@action:button", "Dark Mode")
                    checked: DarkModeControl.darkMode
                    onToggled: DarkModeControl.darkMode = checked
                }

                PlasmaComponents3.Button {
                    id: kcmButton
                    visible: KConfig.KAuthorized.authorizeControlModule("kcm_landingpage")

                    icon.name: "configure"
                    text: i18n("Configure…")

                    Layout.alignment: Qt.AlignRight

                    KeyNavigation.up: root.KeyNavigation.up
                    KeyNavigation.backtab: darkModeSwitch
                    KeyNavigation.left: darkModeSwitch

                    Keys.onPressed: (event) => {
                        if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                            animateClick();
                            event.accepted = true
                        }
                    }
                    onClicked: KCMLauncher.openSystemSettings("kcm_landingpage")
                }
            }

            PlasmaComponents3.Label {
                id: transitionLabel
                text: i18nc("Describes what theme dark mode toggle would switch to", "Switch to %1", DarkModeControl.otherTheme)
                textFormat: Text.PlainText

                opacity: 0.75
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
