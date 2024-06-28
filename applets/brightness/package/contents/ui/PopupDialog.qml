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

PlasmaExtras.Representation {
    id: dialog

    readonly property Item firstItemAfterScreenBrightnessRepeater: keyboardBrightnessSlider.visible ? keyboardBrightnessSlider : keyboardBrightnessSlider.KeyNavigation.down
    KeyNavigation.down: screenBrightnessRepeater.count > 0 ? screenBrightnessRepeater.itemAt(0) : firstItemAfterScreenBrightnessRepeater

    contentItem: PlasmaComponents3.ScrollView {
        id: scrollView

        focus: false

        function positionViewAtItem(item) {
            if (!PlasmaComponents3.ScrollBar.vertical.visible) {
                return;
            }
            const rect = brightnessList.mapFromItem(item, 0, 0, item.width, item.height);
            if (rect.y < scrollView.contentItem.contentY) {
                scrollView.contentItem.contentY = rect.y;
            } else if (rect.y + rect.height > scrollView.contentItem.contentY + scrollView.height) {
                scrollView.contentItem.contentY = rect.y + rect.height - scrollView.height;
            }
        }

        Column {
            id: brightnessList

            spacing: Kirigami.Units.smallSpacing * 2

            readonly property Item firstHeaderItem: {
                if (screenBrightnessRepeater.count > 0) {
                    return screenBrightnessRepeater.itemAt(0);
                } else if (keyboardBrightnessSlider.visible) {
                    return keyboardBrightnessSlider;
                }
                return null;
            }
            readonly property Item lastHeaderItem: {
                if (keyboardBrightnessSlider.visible) {
                    return keyboardBrightnessSlider;
                } else if (screenBrightnessRepeater.count > 0) {
                    return screenBrightnessRepeater.itemAt(screenBrightnessRepeater.count - 1);
                }
                return null;
            }

            Repeater {
                id: screenBrightnessRepeater
                model: screenBrightnessControl.displays

                BrightnessItem {
                    id: screenBrightnessSlider

                    required property int index
                    required property string displayId
                    required property string label
                    required property int brightness
                    required property int maxBrightness

                    width: scrollView.availableWidth

                    icon.name: "video-display-brightness"
                    text: screenBrightnessRepeater.count === 1 ? i18n("Display Brightness") : i18n("Display Brightness - %1", label)
                    type: BrightnessItem.Type.Screen
                    value: brightness
                    maximumValue: maxBrightness

                    KeyNavigation.up: screenBrightnessRepeater.itemAt(index - 1) ?? dialog.KeyNavigation.up
                    KeyNavigation.down: screenBrightnessRepeater.itemAt(index + 1) ?? firstItemAfterScreenBrightnessRepeater
                    KeyNavigation.backtab: screenBrightnessRepeater.itemAt(index - 1) ?? dialog.KeyNavigation.backtab
                    KeyNavigation.tab: KeyNavigation.down

                    stepSize: maxBrightness/100

                    onMoved: screenBrightnessControl.setBrightness(displayId, value)
                    onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)
                }
            }

            BrightnessItem {
                id: keyboardBrightnessSlider

                width: scrollView.availableWidth

                icon.name: "input-keyboard-brightness"
                text: i18n("Keyboard Brightness")
                type: BrightnessItem.Type.Keyboard
                value: keyboardBrightnessControl.brightness
                maximumValue: keyboardBrightnessControl.brightnessMax
                visible: keyboardBrightnessControl.isBrightnessAvailable

                KeyNavigation.up: screenBrightnessRepeater.itemAt(screenBrightnessRepeater.count - 1) ?? dialog.KeyNavigation.up
                KeyNavigation.down: keyboardColorItem.visible ? keyboardColorItem : keyboardColorItem.KeyNavigation.down
                KeyNavigation.backtab: KeyNavigation.up
                KeyNavigation.tab: KeyNavigation.down

                onMoved: keyboardBrightnessControl.brightness = value
                onActiveFocusChanged: if (activeFocus) scrollView.positionViewAtItem(this)

                // Manually dragging the slider around breaks the binding
                Connections {
                    target: keyboardBrightnessControl
                    function onBrightnessChanged() {
                        keyboardBrightnessSlider.value = keyboardBrightnessControl.brightness;
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

                text: i18n("Night Light")
            }

        }
    }
}

