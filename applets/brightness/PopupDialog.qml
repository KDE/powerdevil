/*
    SPDX-FileCopyrightText: 2011 Viranch Mehta <viranch.mehta@gmail.com>
    SPDX-FileCopyrightText: 2013-2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023-2024 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick

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

        Column {
            id: brightnessList

            spacing: Kirigami.Units.smallSpacing * 2

            Repeater {
                id: screenBrightnessRepeater
                model: dialog.screenBrightnessControl.displays

                property Item firstSlider: screenBrightnessRepeater.itemAt(0)
                property Item lastSlider: screenBrightnessRepeater.itemAt(count - 1)

                BrightnessItem {
                    id: screenBrightnessSlider

                    required property int index
                    required property string displayName
                    required property string label
                    required property int brightness
                    required property int maxBrightness

                    property Item previousSlider: screenBrightnessRepeater.itemAt(index - 1)
                    property Item nextSlider: screenBrightnessRepeater.itemAt(index + 1)

                    width: scrollView.availableWidth

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

                    onMoved: dialog.screenBrightnessControl.setBrightness(displayName, value)
                    onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)
                }

                // itemAt() doesn't cause bindings to be updated when the underlying items change,
                // so let's do it by ourselves
                onItemAdded: (index, item) => {
                    if (index == 0) {
                        firstSlider = item;
                    }
                    if (index > 0) {
                        itemAt(index - 1).nextSlider = item;
                    }
                    if (index + 1 < count) {
                        itemAt(index + 1).previousSlider = item;
                    }
                    if (index + 1 == count) {
                        lastSlider = item;
                    }
                }
                onItemRemoved: (index, item) => {
                    if (item == firstSlider) {
                        firstSlider = itemAt(0);
                    }
                    if (index > 0) {
                        itemAt(index - 1).nextSlider = itemAt(index);
                    }
                    if (index + 1 < count) {
                        itemAt(index + 1).previousSlider = itemAt(index);
                    }
                    if (item == lastSlider) {
                        lastSlider = itemAt(count - 1);
                    }
                }
            }

            BrightnessItem {
                id: keyboardBrightnessSlider

                width: scrollView.availableWidth

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

                onMoved: dialog.keyboardBrightnessControl.brightness = value
                onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)

                // Manually dragging the slider around breaks the binding
                Connections {
                    target: dialog.keyboardBrightnessControl
                    function onBrightnessChanged() {
                        keyboardBrightnessSlider.value = dialog.keyboardBrightnessControl.brightness;
                    }
                }
            }

            KeyboardColorItem {
                id: keyboardColorItem

                width: scrollView.availableWidth

                KeyNavigation.up: keyboardBrightnessSlider.visible ? keyboardBrightnessSlider : keyboardBrightnessSlider.KeyNavigation.up
                KeyNavigation.down: nightLightItem
                KeyNavigation.backtab: KeyNavigation.up
                KeyNavigation.tab: KeyNavigation.down

                text: i18n("Keyboard Color")
            }

            NightLightItem {
                id: nightLightItem

                width: scrollView.availableWidth

                KeyNavigation.up: keyboardColorItem.visible ? keyboardColorItem : keyboardColorItem.KeyNavigation.up
                KeyNavigation.down: darkModeItem
                KeyNavigation.tab: KeyNavigation.down
                KeyNavigation.backtab: KeyNavigation.up

                text: i18n("Night Light")
                nightLightControl: dialog.nightLightControl
            }

            DarkModeItem {
                id: darkModeItem

                width: scrollView.availableWidth
                text: i18n("Theme")
            }

        }
    }
}

