/*
    SPDX-FileCopyrightText: 2011 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2011 Viranch Mehta <viranch.mehta@gmail.com>
    SPDX-FileCopyrightText: 2013-2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2021-2022 ivan tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.coreaddons as KCoreAddons
import org.kde.kcmutils // KCMLauncher
import org.kde.config // KAuthorized
import org.kde.notification
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

import org.kde.plasma.private.brightnesscontrolplugin

PlasmoidItem {
    id: brightnessAndColorControl

    readonly property bool inPanel: (Plasmoid.location === PlasmaCore.Types.TopEdge
        || Plasmoid.location === PlasmaCore.Types.RightEdge
        || Plasmoid.location === PlasmaCore.Types.BottomEdge
        || Plasmoid.location === PlasmaCore.Types.LeftEdge)

    NightLightControl {
        id: nightLightControl
    }
    ScreenBrightnessControl {
        id: screenBrightnessControl
        isSilent: brightnessAndColorControl.expanded
    }
    KeyboardBrightnessControl {
        id: keyboardBrightnessControl
        isSilent: brightnessAndColorControl.expanded
    }

    property bool isNightLightActive: nightLightControl.running && nightLightControl.currentTemperature != 6500
    property bool isNightLightInhibited: nightLightControl.inhibited && nightLightControl.targetTemperature != 6500
    property int keyboardBrightnessPercent: keyboardBrightnessControl.brightnessMax ? Math.round(100 * keyboardBrightnessControl.brightness / keyboardBrightnessControl.brightnessMax) : 0

    function symbolicizeIconName(iconName) {
        const symbolicSuffix = "-symbolic";
        if (iconName.endsWith(symbolicSuffix)) {
            return iconName;
        }

        return iconName + symbolicSuffix;
    }

    switchWidth: Kirigami.Units.gridUnit * 10
    switchHeight: Kirigami.Units.gridUnit * 10

    Plasmoid.title: i18n("Brightness and Color")

    LayoutMirroring.enabled: Qt.application.layoutDirection == Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Plasmoid.status: {
        return screenBrightnessControl.isBrightnessAvailable || keyboardBrightnessControl.isBrightnessAvailable || isNightLightActive || isNightLightInhibited ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus;
    }

    // QAbstractItemModel doesn't provide bindable properties for QML, let's make sure
    // toolTipMainText gets updated anyway by (re)setting a variable used in the binding
    Connections {
        id: displayModelConnections
        target: screenBrightnessControl.displays
        property var screenBrightnessInfo: []

        function update() {
            const [labelRole, brightnessRole, maxBrightnessRole] = ["label", "brightness", "maxBrightness"].map(
                (roleName) => target.KItemModels.KRoleNames.role(roleName));

            screenBrightnessInfo = [...Array(target.rowCount()).keys()].map((i) => { // for each display index
                const modelIndex = target.index(i, 0);
                return {
                    label: target.data(modelIndex, labelRole),
                    brightness: target.data(modelIndex, brightnessRole),
                    maxBrightness: target.data(modelIndex, maxBrightnessRole),
                };
            });
        }
        function onDataChanged() { update(); }
        function onModelReset() { update(); }
        function onRowsInserted() { update(); }
        function onRowsMoved() { update(); }
        function onRowsRemoved() { update(); }
    }
    toolTipMainText: {
        const parts = [];
        for (const screen of displayModelConnections.screenBrightnessInfo) {
            const brightnessPercent = screen.maxBrightness ? Math.round(100 * screen.brightness / screen.maxBrightness) : 0
            const text = displayModelConnections.screenBrightnessInfo.length === 1
                ? i18n("Screen brightness at %1%", brightnessPercent)
                : i18nc("Brightness of named display at percentage", "Brightness of %1 at %2%", screen.label, brightnessPercent);
            parts.push(text);
        }

        if (keyboardBrightnessControl.isBrightnessAvailable) {
            parts.push(i18n("Keyboard brightness at %1%", keyboardBrightnessPercent));
        }

        if (nightLightControl.enabled) {
            if (!nightLightControl.running) {
                if (nightLightControl.inhibitedFromApplet) {
                    parts.push(i18nc("Status", "Night Light suspended; middle-click to resume"));
                } else {
                    parts.push(i18nc("Status", "Night Light suspended"));
                }
            } else if (nightLightControl.currentTemperature != 6500) {
                if (nightLightControl.currentTemperature == nightLightControl.targetTemperature) {
                    if (nightLightControl.daylight) {
                        parts.push(i18nc("Status", "Night Light at day color temperature"));
                    } else {
                        parts.push(i18nc("Status", "Night Light at night color temperature"));
                    }
                } else {
                    const endTime = new Date(nightLightControl.currentTransitionEndTime).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                    if (nightLightControl.daylight) {
                        parts.push(i18nc("Status; placeholder is a time", "Night Light in morning transition (complete by %1)", endTime));
                    } else {
                        parts.push(i18nc("Status; placeholder is a time", "Night Light in evening transition (complete by %1)", endTime));
                    }
                }
            }
        }

        return parts.join("\n");
    }
    Connections {
        target: screenBrightnessControl
    }

    toolTipSubText: {
        const parts = [];
        if (nightLightControl.enabled)
            if (nightLightControl.currentTemperature == nightLightControl.targetTemperature) {
                const startTime = new Date(nightLightControl.scheduledTransitionStartTime).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
                if (nightLightControl.daylight) {
                    parts.push(i18nc("Status; placeholder is a time", "Night Light evening transition scheduled for %1", startTime));
                } else {
                    parts.push(i18nc("Status; placeholder is a time", "Night Light morning transition scheduled for %1", startTime));
                }
            }
        if (screenBrightnessControl.isBrightnessAvailable) {
            parts.push(i18n("Scroll to adjust screen brightness"));
        }
        if (nightLightControl.enabled && nightLightControl.running) {
            parts.push(i18n("Middle-click to suspend Night Light"));
        }
        return parts.join("\n");
    }

    Plasmoid.icon: {
        let iconName = "brightness-high";

        if (nightLightControl.enabled) {
            if (!nightLightControl.running) {
                iconName = "redshift-status-off";
            } else if (nightLightControl.currentTemperature != 6500) {
                if (nightLightControl.daylight) {
                    iconName = "redshift-status-day";
                } else {
                    iconName = "redshift-status-on";
                }
            }
        }

        if (inPanel) {
            return symbolicizeIconName(iconName);
        }

        return iconName;
    }

    compactRepresentation: CompactRepresentation {

        onWheel: wheel => {
            if (!screenBrightnessControl.isBrightnessAvailable) {
                return;
            }
            const delta = (wheel.inverted ? -1 : 1) * (wheel.angleDelta.y ? wheel.angleDelta.y : -wheel.angleDelta.x);

            if (Math.abs(delta) < 120) {
                // Touchpad scrolling
                screenBrightnessControl.adjustBrightnessRatio((delta/120) * 0.05);
            } else if (wheel.modifiers & Qt.ShiftModifier) {
                // Discrete/wheel scrolling - round to next small step (e.g. percentage point)
                screenBrightnessControl.adjustBrightnessStep(
                    delta < 0 ? ScreenBrightnessControl.DecreaseSmall : ScreenBrightnessControl.IncreaseSmall);
            } else {
                // Discrete/wheel scrolling - round to next large step (e.g. 5%, 10%)
                screenBrightnessControl.adjustBrightnessStep(
                    delta < 0 ? ScreenBrightnessControl.Decrease : ScreenBrightnessControl.Increase);
            }
        }

        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        property bool wasExpanded: false
        onPressed: wasExpanded = brightnessAndColorControl.expanded
        onClicked: mouse => {
            if (mouse.button == Qt.MiddleButton) {
                if (nightLightControl.enabled) {
                    nightLightControl.toggleInhibition();
                }
            } else {
                brightnessAndColorControl.expanded = !wasExpanded;
            }
        }
    }

    fullRepresentation: PopupDialog {
        id: dialogItem

        readonly property var appletInterface: brightnessAndColorControl

        Layout.minimumWidth: Kirigami.Units.gridUnit * 10
        Layout.maximumWidth: Kirigami.Units.gridUnit * 80
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20

        Layout.minimumHeight: Kirigami.Units.gridUnit * 10
        Layout.maximumHeight: Kirigami.Units.gridUnit * 40
        Layout.preferredHeight: implicitHeight

    } // todo

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            id: configureNightLight
            icon.name: "configure"
            text: i18nc("@action:inmenu", "Configure Night Light…")
            visible: KAuthorized.authorize("kcm_nightlight")
            priority: PlasmaCore.Action.LowPriority
            onTriggered: KCMLauncher.openSystemSettings("kcm_nightlight")
        }
    ]

    // Remove configure action - applet's config is empty, and it also handles
    // brightness; replacing with configureNightLight is inappropriate
    Component.onCompleted: Plasmoid.removeInternalAction("configure")
}
