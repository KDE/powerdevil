/*
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami

RowLayout {
    id: root

    required property bool inhibitionActive
    required property bool inhibitionAllowed
    required property string text
    property string subtext: ""
    property alias iconSource: iconItem.source

    signal inhibitionAllowedRequested(bool allowed)

    spacing: Kirigami.Units.smallSpacing

    PlasmaComponents3.Switch {
        id: control
        Layout.fillWidth: true
        checked: root.inhibitionActive
        focus: true
        font: Kirigami.Theme.smallFont
        spacing: Kirigami.Units.smallSpacing

        text: (root.subtext ? (root.text + "\n" + root.subtext) : root.text)

        KeyNavigation.up: root.KeyNavigation.up
        KeyNavigation.down: root.KeyNavigation.down
        KeyNavigation.tab: root.KeyNavigation.down
        KeyNavigation.backtab: root.KeyNavigation.backtab

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                event.accepted = true;
                clicked();
            }
        }

        onToggled: {
            inhibitionAllowedRequested(checked);
        }

        Connections {
            target: root
            function onInhibitionAllowedChanged() {
                control.checked = Qt.binding(() => root.inhibitionAllowed);
            }
        }
    }

    PlasmaComponents3.BusyIndicator {
        id: busyIndicator
        Layout.preferredHeight: control.implicitHeight

        // show busyIndicator only after a certain timeout
        state: (control.checked === root.inhibitionActive ? "hidden" : "visible")
        states: [
            State {
                name: "hidden"
                PropertyChanges { target: busyIndicator; visible: false }
            },
            State {
                name: "visible"
                PropertyChanges { target: busyIndicator; visible: true }
            }
        ]
        transitions: [
            Transition {
                from: "hidden"
                to: "visible"
                SequentialAnimation {
                    PauseAnimation { duration: 1000 }
                    PropertyAction { property: "visible" }
                }
            }
        ]
    }

    Kirigami.Icon {
        id: iconItem
        Layout.preferredWidth: Kirigami.Units.iconSizes.small
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
        visible: valid
    }
}
