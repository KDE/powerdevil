/*
    SPDX-FileCopyrightText: 2012-2013 Daniel Nicoletti <dantti12@gmail.com>
    SPDX-FileCopyrightText: 2013, 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.notification
import org.kde.kwindowsystem as KWindowSystem
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.ksvg as KSvg
import org.kde.kirigami as Kirigami

PlasmaComponents3.ItemDelegate {
    id: root

    property bool pluggedIn

    signal inhibitionChangeRequested(bool inhibit)

    property bool isManuallyInhibited
    property bool isManuallyInhibitedError
    // List of currently requested power management inhibitions (applications that
    // are blocking sleep and screen locking). Not all are necessarily active.
    //
    // type: [{
    //  appName: string,
    //  prettyName: string,
    //  icon: string,
    //  reason: string,
    //  behaviors: array<string>, // what's inhibited: "idle", "sleep", both, or any future inhibition types
    //  active: bool,
    //  allowed: bool,
    // }]
    property var requestedInhibitions: []
    property bool inhibitsLidAction

    readonly property var activeInhibitions: root.requestedInhibitions.filter((inh) => inh.active)

    readonly property int numActiveIdleInhibitions: root.activeInhibitions.reduce((num, inh) => inh.behaviors.includes("idle") ? (num + 1) : num, 0)
    readonly property int numActiveSleepInhibitions: root.activeInhibitions.reduce((num, inh) => inh.behaviors.includes("sleep") ? (num + 1) : num, 0)
    readonly property int numActiveIdleOrSleepInhibitions: root.activeInhibitions.reduce((num, inh) => (inh.behaviors.includes("idle") || inh.behaviors.includes("sleep")) ? (num + 1) : num, 0)

    background.visible: highlighted
    highlighted: activeFocus
    hoverEnabled: false
    text: i18nc("@title:group", "Sleep and Screen Locking after Inactivity")

    Notification {
        id: inhibitionError
        componentName: "plasma_workspace"
        eventId: "warning"
        iconName: "system-suspend-uninhibited"
        title: i18n("Power Management")
    }

    Accessible.description: {
        if (root.numActiveIdleInhibitions > 0 && root.numActiveSleepInhibitions > 0) {
            return i18nc("accessible-only status text", "Automatic sleep and screen locking are currently blocked.");
        } else if (root.numActiveIdleInhibitions > 0) {
            return i18nc("accessible-only status text", "Screen locking is currently blocked. Sleep is automatic after inactivity.");
        } else if (root.numActiveSleepInhibitions > 0) {
            return i18nc("accessible-only status text", "Sleep is currently blocked. Screen locking is automatic after inactivity.");
        } else { // no active inhibitions
            return i18nc("accessible-only status text", "Sleep and screen locking are automatic after inactivity.");
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.gridUnit

        Kirigami.Icon {
            id: icon
            source: (root.isManuallyInhibited || root.numActiveIdleOrSleepInhibitions > 0) ? "system-suspend-inhibited" : "system-suspend-uninhibited"
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Kirigami.Units.smallSpacing

            // UI to undo manually inhibit sleep and screen locking
            PlasmaComponents3.Switch {
                Layout.fillWidth: true
                text: i18nc("@action:button Block sleep and screen locking after inactivity", "Manually Block Sleep and Screen Locking")

                Keys.onPressed: (event) => {
                    if (event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                        click();
                    }
                }

                onToggled: {
                    inhibitionChangeRequested(!root.isManuallyInhibited);
                }

                Connections {
                    target: root
                    function onIsManuallyInhibitedErrorChanged() {
                        if (root.isManuallyInhibitedError) {
                            root.isManuallyInhibitedError = false;
                            if (!root.isManuallyInhibited) {
                                inhibitionError.text = i18n("Failed to unblock automatic sleep and screen locking");
                                inhibitionError.sendEvent();
                            } else {
                                inhibitionError.text = i18n("Failed to block automatic sleep and screen locking");
                                inhibitionError.sendEvent();
                            }
                        }
                    }
                }
            }

            InhibitionHint {
                Layout.fillWidth: true
                iconSource: "dialog-warning"

                // EU Regulation 2023/826 requires a warning if user disables auto suspend.
                visible: root.isManuallyInhibited
                text: i18nc("@label corresponding to Manually Block Sleep and Screen Locking toggle", "This will result in higher energy consumption.")
                Accessible.description: i18nc("@label accessible - the sentence about higher energy consumption must be included (EU regulations)", "You have manually blocked automatic sleep and screen locking. This will result in higher energy consumption.")
            }

            InhibitionHint {
                Layout.fillWidth: true
                visible: root.inhibitsLidAction
                iconSource: "computer-laptop"
                text: i18nc("Minimize the length of this string as much as possible", "Your laptop is configured not to sleep when closing the lid while an external monitor is connected.")
            }

            Repeater {
                model: root.requestedInhibitions

                InhibitionHint {
                    required property string icon
                    required property string appName
                    required property string prettyName
                    required property string reason
                    required property bool active
                    required property bool allowed
                    required property var behaviors

                    Layout.fillWidth: true

                    iconSource: icon || (KWindowSystem.KWindowSystem.isPlatformWayland ? "wayland" : "xorg")
                    text: {
                        const why = reason ?? i18nc("An application's reason for inhibiting sleep and/or screen locking", "Unknown reason.");

                        const isIdleInhibition = behaviors.includes("idle");
                        const isSleepInhibition = behaviors.includes("sleep");

                        if (!allowed) {
                            if (isIdleInhibition && isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) has been prevented from blocking this inactivity behavior. The user can choose to allow it again.", "%1 wants to block sleep and screen locking. (%2)", prettyName, why);
                            } else if (isIdleInhibition) {
                                return i18nc("Application (1) with reason (2) has been prevented from blocking this inactivity behavior. The user can choose to allow it again.", "%1 wants to block screen locking. (%2)", prettyName, why);
                            } else if (isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) has been prevented from blocking this inactivity behavior. The user can choose to allow it again.", "%1 wants to block sleep. (%2)", prettyName, why);
                            }
                        } else if (active && root.isManuallyInhibited) {
                            if (isIdleInhibition && isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, in addition to the user's manual block switch.", "%1 is also blocking sleep and screen locking. (%2)", prettyName, why);
                            } else if (isIdleInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, in addition to the user's manual block switch.", "%1 is also blocking screen locking. (%2)", prettyName, why);
                            } else if (isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, in addition to the user's manual block switch.", "%1 is also blocking sleep. (%2)", prettyName, why);
                            }
                        } else if (active) {
                            if (isIdleInhibition && isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, the user can choose to unblock it.", "%1 is blocking sleep and screen locking. (%2)", prettyName, why);
                            } else if (isIdleInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, the user can choose to unblock it.", "%1 is blocking screen locking. (%2)", prettyName, why);
                            } else if (isSleepInhibition) {
                                return i18nc("Application (1) with reason (2) is currently blocking this inactivity behavior, the user can choose to unblock it.", "%1 is blocking sleep. (%2)", prettyName, why);
                            }
                        }
                    }

                    PlasmaComponents3.Button {
                        id: suppressInhibitionMenuButton
                        visible: allowed
                        text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Unblock")
                        icon.name: "edit-delete-remove"
                        onClicked: pmControl.setInhibitionAllowed(appName, reason, false)
                    }

                    PlasmaComponents3.Button {
                        id: allowInhibitionMenuButton
                        visible: !allowed
                        text: i18nc("@action:button Undo preventing an app from blocking automatic sleep and screen locking after inactivity", "Allow")
                        icon.name: "dialog-ok"
                        onClicked: pmControl.setInhibitionAllowed(appName, reason, true)
                    }
                }
            }
        }
    }
}
