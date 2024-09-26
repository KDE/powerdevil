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
    // List of active power management inhibitions (applications that are
    // blocking sleep and screen locking).
    //
    // type: [{
    //  Name: string,
    //  PrettyName: string,
    //  Icon: string,
    //  Reason: string,
    // }]
    property var inhibitions: []
    property var blockedInhibitions: []
    property bool inhibitsLidAction

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

    Accessible.description: pmStatusLabel.text
    Accessible.role: Accessible.CheckBox

    contentItem: RowLayout {
        spacing: Kirigami.Units.gridUnit

        Kirigami.Icon {
            id: icon
            source: root.isManuallyInhibited || root.inhibitions.length > 0 ? "system-suspend-inhibited" : "system-suspend-uninhibited"
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: root.text
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                PlasmaComponents3.Label {
                    id: pmStatusLabel
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    text: root.isManuallyInhibited || root.inhibitions.length > 0 ? i18nc("Sleep and Screen Locking after Inactivity", "Blocked") : i18nc("Sleep and Screen Locking after Inactivity", "Automatic")
                    textFormat: Text.PlainText
                }
            }

            // list of inhibitions
            ColumnLayout {
                id: inhibitionReasonsLayout

                Layout.fillWidth: true

                InhibitionHint {
                    readonly property var pmControl: root.pmControl

                    Layout.fillWidth: true
                    visible: root.inhibitsLidAction
                    iconSource: "computer-laptop"
                    text: i18nc("Minimize the length of this string as much as possible", "Your laptop is configured not to sleep when closing the lid while an external monitor is connected.")
                }

                // list of automatic inhibitions
                PlasmaComponents3.Label {
                    id: inhibitionExplanation
                    Layout.fillWidth: true
                    visible: root.inhibitions.length > 1
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 3
                    text: i18np("%1 application is currently blocking sleep and screen locking:",
                                "%1 applications are currently blocking sleep and screen locking:",
                                root.inhibitions.length)
                    textFormat: Text.PlainText
                }

                Repeater {
                    model: root.inhibitions

                    InhibitionHint {
                        property string icon: modelData.Icon
                            || (KWindowSystem.KWindowSystem.isPlatformWayland ? "wayland" : "xorg")
                        property string app: modelData.Name
                        property string name: modelData.PrettyName
                        property string reason: modelData.Reason
                        property bool permanentlyBlocked: {
                            return root.blockedInhibitions.some(function (blockedInhibition) {
                                return blockedInhibition.Name === app && blockedInhibition.Reason === reason && blockedInhibition.Permanently;
                            });
                        }

                        Layout.fillWidth: true
                        iconSource: icon
                        text: {
                            if (root.inhibitions.length === 1) {
                                if (reason && name) {
                                    return i18n("%1 is currently blocking sleep and screen locking (%2)", name, reason)
                                } else if (name) {
                                    return i18n("%1 is currently blocking sleep and screen locking (unknown reason)", name)
                                } else if (reason) {
                                    return i18n("An application is currently blocking sleep and screen locking (%1)", reason)
                                } else {
                                    return i18n("An application is currently blocking sleep and screen locking (unknown reason)")
                                }
                            } else {
                                if (reason && name) {
                                    return i18nc("Application name: reason for preventing sleep and screen locking", "%1: %2", name, reason)
                                } else if (name) {
                                    return i18nc("Application name: reason for preventing sleep and screen locking", "%1: unknown reason", name)
                                } else if (reason) {
                                    return i18nc("Application name: reason for preventing sleep and screen locking", "Unknown application: %1", reason)
                                } else {
                                    return i18nc("Application name: reason for preventing sleep and screen locking", "Unknown application: unknown reason")
                                }
                            }
                        }

                        Item {
                            visible: !permanentlyBlocked
                            width: blockMenuButton.width
                            height: blockMenuButton.height

                            PlasmaComponents3.Button {
                                id: blockMenuButton
                                text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Unblock")
                                icon.name: "edit-delete-remove"
                                Accessible.role: Accessible.ButtonMenu
                                onClicked: blockMenuButtonMenu.open()
                            }

                            PlasmaExtras.Menu {
                                id: blockMenuButtonMenu

                                PlasmaExtras.MenuItem {
                                    text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Only this time")
                                    onClicked: pmControl.blockInhibition(app, reason, false)
                                }

                                PlasmaExtras.MenuItem {
                                    text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Every time for this app and reason")
                                    onClicked: pmControl.blockInhibition(app, reason, true)
                                }
                            }
                        }

                        Item {
                            visible: permanentlyBlocked
                            width: blockButton.width
                            height: blockButton.height

                            PlasmaComponents3.Button {
                                id: blockButton
                                text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Unblock")
                                icon.name: "edit-delete-remove"
                                onClicked: pmControl.blockInhibition(app, reason, true)
                            }
                        }
                    }
                }

                // UI to undo manually inhibit sleep and screen locking
                InhibitionHint {
                    visible: root.isManuallyInhibited

                    iconSource: "user"
                    text: i18nc("Minimize the length of this string as much as possible", "You have manually blocked sleep and screen locking.")

                    PlasmaComponents3.Button {
                        id: manualUninhibitionButton
                        Layout.alignment: Qt.AlignRight
                        text: i18nc("@action:button Undo blocking sleep and screen locking after inactivity", "Unblock")
                        icon.name: "edit-delete-remove"

                        Keys.onPressed: (event) => {
                            if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                                click();
                            }
                        }

                        onClicked: {
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
                }

                // list of blocked inhibitions
                PlasmaComponents3.Label {
                    id: blockedInhibitionExplanation
                    Layout.fillWidth: true
                    visible: root.blockedInhibitions.length > 1
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 3
                    text: i18np("%1 application has been prevented from blocking sleep and screen locking:",
                                "%1 applications have been prevented from blocking sleep and screen locking:",
                                root.blockedInhibitions.length)
                    textFormat: Text.PlainText
                }

                Repeater {
                    model: root.blockedInhibitions

                    InhibitionHint {
                        property string icon: modelData.Icon
                            || (KWindowSystem.isPlatformWayland ? "wayland" : "xorg")
                        property string app: modelData.Name
                        property string name: modelData.PrettyName
                        property string reason: modelData.Reason
                        property bool permanently: modelData.Permanently
                        property bool temporarilyUnblocked: {
                            return root.inhibitions.some(function (inhibition) {
                                return inhibition.Name === app && inhibition.Reason === reason;
                            });
                        }
                        visible: !temporarilyUnblocked

                        Layout.fillWidth: true
                        iconSource: icon
                        text: {
                            if (root.blockedInhibitions.length === 1) {
                                return i18nc("Application name; reason", "%1 has been prevented from blocking sleep and screen locking for %2", name, reason)
                            } else {
                                return i18nc("Application name: reason for preventing sleep and screen locking", "%1: %2", name, reason)
                            }
                        }

                        Item {
                            visible: permanently
                            width: unblockMenuButton.width
                            height: unblockMenuButton.height

                            PlasmaComponents3.Button {
                                id: unblockMenuButton
                                text: i18nc("@action:button Undo preventing an app from blocking automatic sleep and screen locking after inactivity", "Block Again")
                                icon.name: "dialog-cancel"
                                Accessible.role: Accessible.ButtonMenu
                                onClicked: unblockButtonMenu.open()
                            }

                            PlasmaExtras.Menu {
                                id: unblockButtonMenu

                                PlasmaExtras.MenuItem {
                                    text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Only this time")
                                    onClicked: pmControl.unblockInhibition(app, reason, false)
                                }

                                PlasmaExtras.MenuItem {
                                    text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Every time for this app and reason")
                                    onClicked: pmControl.unblockInhibition(app, reason, true)
                                }
                            }
                        }

                        Item {
                            visible: !permanently
                            width: unblockButton.width
                            height: unblockButton.height

                            PlasmaComponents3.Button {
                                id: unblockButton
                                text: i18nc("@action:button Undo preventing an app from blocking automatic sleep and screen locking after inactivity", "Block Again")
                                icon.name: "dialog-cancel"
                                onClicked: pmControl.unblockInhibition(app, reason, false)
                            }
                        }
                    }
                }

                // UI to manually inhibit sleep and screen locking
                InhibitionHint {
                    visible: !root.isManuallyInhibited

                    PlasmaComponents3.Button {
                        id: manualInhibitionButton
                        Layout.alignment: Qt.AlignRight
                        text: i18nc("@action:button Block sleep and screen locking after inactivity", "Manually Block")
                        icon.name: "dialog-cancel"

                        Keys.onPressed: (event) => {
                            if (event.key == Qt.Key_Space || event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                                click();
                            }
                        }

                        onClicked: {
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
                }
            }
        }
    }
}
