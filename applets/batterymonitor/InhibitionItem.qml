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
    //  what: array<string>, // what's inhibited: "idle", "sleep", both, or any future inhibition types
    //  active: bool,
    //  allowed: bool,
    // }]
    property var requestedInhibitions: []
    property bool inhibitsLidAction

    readonly property bool hasRequestedIdleInhibitions: root.requestedInhibitions.some((inh) => inh.what.includes("idle"))
    readonly property bool hasRequestedSleepInhibitions: root.requestedInhibitions.some((inh) => inh.what.includes("sleep"))

    readonly property bool hasActiveIdleInhibitions: root.requestedInhibitions.some((inh) => inh.active && inh.what.includes("idle"))
    readonly property bool hasActiveSleepInhibitions: root.requestedInhibitions.some((inh) => inh.active && inh.what.includes("sleep"))
    readonly property bool hasActiveInhibitions: root.hasActiveIdleInhibitions || root.hasActiveSleepInhibitions

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
        if (root.isManuallyInhibited || (root.hasActiveIdleInhibitions && root.hasActiveSleepInhibitions)) {
            return i18nc("Sleep and Screen Locking after Inactivity", "Blocked");
        } else if (root.hasActiveIdleInhibitions) {
            return i18n("Automatic Sleep; Screen Locking is blocked");
        } else if (root.hasActiveSleepInhibitions) {
            return i18n("Automatic Screen Locking; Sleep is blocked");
        } else {
            return i18nc("Sleep and Screen Locking after Inactivity", "Automatic");
        }
    }
    Accessible.role: Accessible.CheckBox

    contentItem: RowLayout {
        spacing: Kirigami.Units.gridUnit

        Kirigami.Icon {
            id: icon
            source: (root.isManuallyInhibited || root.hasActiveInhibitions) ? "system-suspend-inhibited" : "system-suspend-uninhibited"
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
                    text: i18nc("Automatic or Blocked", "Sleep after Inactivity")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                PlasmaComponents3.Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    text: (root.isManuallyInhibited || root.hasActiveSleepInhibitions) ? i18nc("Sleep after Inactivity", "Blocked") : i18nc("Sleep after Inactivity", "Automatic")
                    textFormat: Text.PlainText
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: i18nc("Automatic or Blocked", "Screen Locking after Inactivity")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                PlasmaComponents3.Label {
                    Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    text: (root.isManuallyInhibited || root.hasActiveIdleInhibitions) ? i18nc("Screen Locking after Inactivity", "Blocked") : i18nc("Screen Locking after Inactivity", "Automatic")
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
                    visible: root.hasRequestedIdleInhibitions || root.hasRequestedSleepInhibitions
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 3
                    text: {
                        if (root.hasRequestedIdleInhibitions && root.hasRequestedSleepInhibitions) {
                            return i18np("%1 application currently wants to block sleep and screen locking:",
                                "%1 applications currently want to block sleep and screen locking:",
                                root.requestedInhibitions.length);
                        } else if (root.hasRequestedIdleInhibitions) {
                            return i18np("%1 application currently wants to block screen locking:",
                                "%1 applications currently want to block screen locking:",
                                root.requestedInhibitions.length);
                        } else { // root.hasRequestedSleepInhibitions
                            return i18np("%1 application currently wants to block sleep:",
                                "%1 applications currently want to block sleep:",
                                root.requestedInhibitions.length);
                        }
                    }
                    textFormat: Text.PlainText
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
                        required property var what

                        Layout.fillWidth: true

                        iconSource: icon || (KWindowSystem.KWindowSystem.isPlatformWayland ? "wayland" : "xorg")
                        text: {
                            const why = reason ?? i18nc("Sleep and/or screen locking inhibition reason if none is provided by the application", "unknown reason");
                            const who = prettyName ?? appName;

                            const isIdleInhibition = what.includes("idle");
                            const isSleepInhibition = what.includes("sleep");

                            var inhibitionBehavior;
                            if (allowed && root.requestedInhibitions.length > 1) { // for 1 exactly, inhibitionExplanation above already says the same thing
                                if (isIdleInhibition && isSleepInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Blocking sleep and screen locking.");
                                } else if (isIdleInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Blocking screen locking.");
                                } else if (isSleepInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Blocking sleep.");
                                }
                            } else if (!allowed) {
                                if (isIdleInhibition && isSleepInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Prevented from blocking sleep and screen locking.");
                                } else if (isIdleInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Prevented from blocking screen locking.");
                                } else if (isSleepInhibition) {
                                    inhibitionBehavior = i18nc("Text below inhibiting application name and reason.", "Prevented from blocking sleep.");
                                }
                            }

                            const whoAndWhy = i18nc("Application name: reason for preventing sleep and screen locking", "%1: %2", who, why);
                            return inhibitionBehavior ? (whoAndWhy + "\n" + inhibitionBehavior) : whoAndWhy;
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
                            text: i18nc("@action:button Undo preventing an app from blocking automatic sleep and screen locking after inactivity", "Block Again")
                            icon.name: "dialog-cancel"
                            onClicked: pmControl.setInhibitionAllowed(appName, reason, true)
                        }
                    }
                }

                // UI to undo manually inhibit sleep and screen locking
                InhibitionHint {
                    visible: root.isManuallyInhibited

                    iconSource: "user"
                    // EU Regulation 2023/826 requires a warning if user disables auto suspend.
                    text: i18nc("Minimize the length of this string as much as possible but the sentence about higher energy consumption must be included (EU regulations)", "You have manually blocked sleep and screen locking. This will result in higher energy consumption.")

                    PlasmaComponents3.Button {
                        id: manualUninhibitionButton
                        Layout.alignment: Qt.AlignRight
                        text: i18nc("@action:button Undo blocking sleep and screen locking after inactivity", "Unblock")
                        icon.name: "edit-delete-remove"

                        Keys.onPressed: (event) => {
                            if (event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
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

                // UI to manually inhibit sleep and screen locking
                InhibitionHint {
                    visible: !root.isManuallyInhibited

                    PlasmaComponents3.Button {
                        id: manualInhibitionButton
                        Layout.alignment: Qt.AlignRight
                        text: i18nc("@action:button Block sleep and screen locking after inactivity", "Manually Block")
                        icon.name: "dialog-cancel"

                        Keys.onPressed: (event) => {
                            if (event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
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
