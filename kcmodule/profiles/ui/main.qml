/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.AbstractKCM {
    id: root

    KCM.ConfigModule.buttons: KCM.ConfigModule.Default | KCM.ConfigModule.Apply

    actions: Kirigami.Action {
        text: i18nc("@action:button", "Advanced Power &Settings…")
        icon.name: "settings-configure"
        enabled: kcm.powerManagementServiceRegistered
        onTriggered: { kcm.push("GlobalConfig.qml"); }
    }

    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: Kirigami.Units.gridUnit * 35
    framedView: false

    Rectangle {
        // We're using AbstractKCM so that the automatic scrolling support of SimpleKCM
        // doesn't get in the way of the "tab content" Flickable below.
        // Use this lovely rectangle instead to still give the view a default background color.
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor
    }

    Kirigami.PlaceholderMessage {
        id: errorMessage
        anchors.centerIn: parent
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        visible: !kcm.powerManagementServiceRegistered
        icon.name: "dialog-error"
        text: i18nc("@text:placeholdermessage", "Power Management settings could not be loaded")
        explanation: kcm.powerManagementServiceErrorReason
    }

    ColumnLayout {
        anchors.fill: parent
        visible: kcm.powerManagementServiceRegistered
        spacing: 0

        Kirigami.NavigationTabBar {
            id: profileTabBar

            Layout.fillWidth: true
            visible: kcm.supportsBatteryProfiles

            actions: [
                Kirigami.Action {
                    text: i18n("On AC Power")
                    icon.name: "battery-full-charging"
                    checked: profileConfigStack.currentIndex == profileConfigAC.StackLayout.index
                    onTriggered: {
                        profileConfigStack.currentIndex = profileConfigAC.StackLayout.index;
                    }
                },
                Kirigami.Action {
                    text: i18n("On Battery")
                    icon.name: "battery-good"
                    checked: profileConfigStack.currentIndex == profileConfigBattery.StackLayout.index
                    onTriggered: {
                        profileConfigStack.currentIndex = profileConfigBattery.StackLayout.index;
                    }
                },
                Kirigami.Action {
                    text: i18n("On Low Battery")
                    icon.name: "battery-low"
                    checked: profileConfigStack.currentIndex == profileConfigLowBattery.StackLayout.index
                    onTriggered: {
                        profileConfigStack.currentIndex = profileConfigLowBattery.StackLayout.index;
                    }
                }
            ]
        }

        Kirigami.Separator {
            visible: profileTabBar.visible
            Layout.fillWidth: true
        }

        Item {
            id: scrollBarContainer
            clip: true

            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: flickable.implicitWidth
            implicitHeight: flickable.implicitHeight

            Flickable {
                id: flickable

                anchors.fill: scrollBarContainer
                implicitWidth: wheelHandler.horizontalStepSize * 10 + leftMargin + rightMargin
                implicitHeight: wheelHandler.verticalStepSize * 10 + topMargin + bottomMargin
                contentWidth: contentItem.childrenRect.width
                contentHeight: contentItem.childrenRect.height

                leftMargin: QQC2.ScrollBar.vertical.visible && QQC2.ScrollBar.vertical.mirrored ? QQC2.ScrollBar.vertical.width : 0
                rightMargin: QQC2.ScrollBar.vertical.visible && !QQC2.ScrollBar.vertical.mirrored ? QQC2.ScrollBar.vertical.width : 0
                bottomMargin: QQC2.ScrollBar.horizontal.visible ? QQC2.ScrollBar.horizontal.height : 0

                Kirigami.WheelHandler {
                    id: wheelHandler
                    target: flickable
                    filterMouseEvents: true
                    keyNavigationEnabled: true
                }

                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    parent: scrollBarContainer
                    height: flickable.height - flickable.topMargin - flickable.bottomMargin
                    x: mirrored ? 0 : flickable.width - width
                    y: flickable.topMargin
                    active: flickable.QQC2.ScrollBar.horizontal.active
                    stepSize: wheelHandler.verticalStepSize / flickable.contentHeight
                }

                QQC2.ScrollBar.horizontal: QQC2.ScrollBar {
                    parent: scrollBarContainer
                    width: flickable.width - flickable.leftMargin - flickable.rightMargin
                    x: flickable.leftMargin
                    y: flickable.height - height
                    active: flickable.QQC2.ScrollBar.vertical.active
                    stepSize: wheelHandler.horizontalStepSize / flickable.contentWidth
                }

                Item {
                    id: tabContentContainer
                    width: Math.max(scrollBarContainer.width - flickable.leftMargin - flickable.rightMargin, profileConfigStack.width)
                    height: Math.max(scrollBarContainer.height - flickable.topMargin - flickable.bottomMargin, profileConfigStack.height)

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            // allow closing popups by clicking outside
                            profileConfigStack.forceActiveFocus();
                        }
                    }

                    StackLayout {
                        id: profileConfigStack
                        anchors.horizontalCenter: tabContentContainer.horizontalCenter
                        anchors.top: tabContentContainer.top
                        anchors.margins: Kirigami.Units.smallSpacing

                        readonly property var profileIds: ["AC", "Battery", "LowBattery"]
                        currentIndex: profileIds.indexOf(kcm.currentProfile)

                        ProfileConfig {
                            id: profileConfigAC
                            profileId: profileConfigStack.profileIds[0]
                        }
                        ProfileConfig {
                            id: profileConfigBattery
                            profileId: profileConfigStack.profileIds[1]
                        }
                        ProfileConfig {
                            id: profileConfigLowBattery
                            profileId: profileConfigStack.profileIds[2]
                        }

                        // Without manually signaling, "checked" in profileTabBar actions
                        // doesn't initialize correctly. Not sure what's going wrong there.
                        Component.onCompleted: { currentIndexChanged(); }
                    }
                }
            }
        }
    }
}
