/*
    SPDX-FileCopyrightText: 2011 Viranch Mehta <viranch.mehta@gmail.com>
    SPDX-FileCopyrightText: 2013-2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023-2024 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

import org.kde.plasma.private.brightnesscontrolplugin

pragma ComponentBehavior: Bound

PlasmaExtras.Representation {
    id: dialog

    required property ScreenBrightnessControl screenBrightnessControl
    required property KeyboardBrightnessControl keyboardBrightnessControl
    required property NightLightItem.NightLightControl nightLightControl

    readonly property Item firstItemAfterScreenBrightnessRepeater: keyboardBrightnessSlider.visible ? keyboardBrightnessSlider : keyboardBrightnessSlider.KeyNavigation.down
    KeyNavigation.down: screenBrightnessRepeater.firstSlider ?? firstItemAfterScreenBrightnessRepeater

    contentItem: PlasmaComponents3.ScrollView {
        id: scrollView
        PlasmaComponents3.ScrollBar.horizontal.visible: false

        focus: false

        function positionViewAtItem(item) {
            if (!PlasmaComponents3.ScrollBar.vertical.visible) {
                return;
            }
            const rect = brightnessList.mapFromItem(item, 0, 0, item.width, item.height);
            const flickable = scrollView.contentItem as Flickable
            if (rect.y < flickable.contentY) {
                flickable.contentY = rect.y;
            } else if (rect.y + rect.height > flickable.contentY + scrollView.height) {
                flickable.contentY = rect.y + rect.height - scrollView.height;
            }
        }

        ColumnLayout {
            id: brightnessList

            width: scrollView.contentItem.width
            spacing: Kirigami.Units.smallSpacing * 2

            Repeater {
                id: screenBrightnessRepeater
                model: dialog.screenBrightnessControl.displays

                property Item firstSlider: null
                property Item lastSlider: null

                BrightnessItem {
                    id: screenBrightnessSlider

                    required property int index
                    required property string displayName
                    required property string label
                    required property int brightness
                    required property int maxBrightness

                    property Item previousSlider: null
                    property Item nextSlider: null

                    Layout.fillWidth: true

                    activeFocusOnTab: true
                    icon.name: "video-display-brightness"
                    text: label
                    type: BrightnessItem.Type.Screen
                    value: brightness
                    minimumValue: 0
                    maximumValue: maxBrightness

                    KeyNavigation.up: previousSlider ?? dialog.KeyNavigation.up
                    KeyNavigation.down: nextSlider ?? dialog.firstItemAfterScreenBrightnessRepeater
                    KeyNavigation.backtab: previousSlider ?? dialog.KeyNavigation.backtab
                    KeyNavigation.tab: KeyNavigation.down

                    stepSize: maxBrightness/100

                    onMoved: value => dialog.screenBrightnessControl.setBrightness(displayName, value)
                    onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)
                }

                // Keep neighboring sliders linked explicitly: itemAt() doesn't notify bindings
                // when items are inserted, removed or reindexed.
                function updateKeyNavigation() {
                    firstSlider = itemAt(0) as BrightnessItem;
                    let previousSlider = firstSlider;
                    for (let i = 1; i < count; ++i) {
                        const slider = itemAt(i) as BrightnessItem;
                        slider.previousSlider = previousSlider;
                        previousSlider.nextSlider = slider;
                        previousSlider = slider;
                    }
                    lastSlider = previousSlider;
                    if (firstSlider) {
                        firstSlider.previousSlider = null;
                        lastSlider.nextSlider = null;
                    }
                }
                onItemAdded: Qt.callLater(updateKeyNavigation)
                onItemRemoved: Qt.callLater(updateKeyNavigation)
            }

            BrightnessItem {
                id: keyboardBrightnessSlider

                Layout.fillWidth: true

                icon.name: "input-keyboard-brightness"
                text: i18n("Keyboard Backlight")
                type: BrightnessItem.Type.Keyboard
                value: dialog.keyboardBrightnessControl.brightness
                maximumValue: dialog.keyboardBrightnessControl.brightnessMax
                visible: dialog.keyboardBrightnessControl.isBrightnessAvailable

                KeyNavigation.up: screenBrightnessRepeater.lastSlider ?? dialog.KeyNavigation.up
                KeyNavigation.down: keyboardColorItem.visible ? keyboardColorItem : keyboardColorItem.KeyNavigation.down
                KeyNavigation.backtab: KeyNavigation.up
                KeyNavigation.tab: KeyNavigation.down

                onMoved: value => dialog.keyboardBrightnessControl.brightness = value
                onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)
            }

            KeyboardColorItem {
                id: keyboardColorItem

                Layout.fillWidth: true

                KeyNavigation.up: keyboardBrightnessSlider.visible ? keyboardBrightnessSlider : keyboardBrightnessSlider.KeyNavigation.up
                KeyNavigation.down: nightLightItem
                KeyNavigation.backtab: KeyNavigation.up
                KeyNavigation.tab: KeyNavigation.down

                text: i18n("Keyboard Color")
            }

            NightLightItem {
                id: nightLightItem

                Layout.fillWidth: true

                KeyNavigation.up: keyboardColorItem.visible ? keyboardColorItem : keyboardColorItem.KeyNavigation.up
                KeyNavigation.down: darkModeItem
                KeyNavigation.tab: KeyNavigation.down
                KeyNavigation.backtab: KeyNavigation.up

                text: i18n("Night Light")
                nightLightControl: dialog.nightLightControl
            }

            DarkModeItem {
                id: darkModeItem

                Layout.fillWidth: true
                text: i18n("Theme")
            }

        }
    }
}

