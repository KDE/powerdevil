/*
 *  SPDX-FileCopyrightText: 2022, 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.powerdevil as PD

Kirigami.FormLayout {
    id: root

    required property string profileId
    required property string profileLabel
    readonly property var profileSettings: kcm.settings["profile" + profileId]

    function formatPercentageText(value) {
        return i18nc(
            "Percentage value example, used for formatting brightness levels in the power management settings",
            "10%"
        ).replace("10", value);
    }

    Connections {
        target: root.profileSettings
        function onAutoSuspendIdleTimeoutSecChanged() {
            suspendComplianceWarning.showIfNeeded();
        }
        function onAutoSuspendActionChanged() {
            suspendComplianceWarning.showIfNeeded();
        }
    }

    //
    // Suspend Session

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Suspend Session")
        visible: (
            autoSuspendActionRow.visible
            || powerButtonActionCombo.visible
            || lidActionCombo.visible
            || triggersLidActionWhenExternalMonitorPresentCheck.visible
            || sleepModeCombo.visible
        )
    }

    Kirigami.InlineMessage {
        Kirigami.FormData.isSection: true
        visible: autoSuspendActionCombo.visible && !autoSuspendActionCombo.isConfiguredValueSupported
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The action you had previously configured to happen when inactive is now unsupported on your system. Please select a different one.")
    }

    RowLayout {
        id: autoSuspendActionRow
        Kirigami.FormData.label: i18nc(
            "@label:combobox Suspend action such as sleep/hibernate to perform when the system is idle",
            "When &inactive:"
        )
        Kirigami.FormData.buddyFor: autoSuspendActionCombo
        visible: kcm.supportedActions["SuspendSession"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        ComboBoxWithIcon {
            id: autoSuspendActionCombo
            Accessible.name: i18nc("@accessible:name:combobox", "Action to perform when the system is idle")

            Layout.fillWidth: true
            implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

            model: kcm.autoSuspendActionModel
            textRole: "name"
            valueRole: "value"

            readonly property bool isConfiguredValueSupported: currentValue === root.profileSettings.autoSuspendAction

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "AutoSuspendAction"
            }
            Component.onCompleted: {
                // indexOfValue() is invalid before onCompleted, so wait until here to bind currentIndex.
                currentIndex = Qt.binding(() => indexOfValue(root.profileSettings.autoSuspendAction));
            }
            onActivated: {
                root.profileSettings.autoSuspendAction = currentValue;
            }
        }

        TimeDurationComboBox {
            id: autoSuspendIdleTimeoutCombo
            durationPromptLabel: autoSuspendActionRow.Kirigami.FormData.label
            durationPromptAcceptsUnits: [DurationPromptDialog.Unit.Seconds, DurationPromptDialog.Unit.Minutes]

            function translateSeconds(n, formatUnit = DurationPromptDialog.Unit.Seconds) {
                return formatUnit == DurationPromptDialog.Unit.Minutes
                    ? translateMinutes(n / 60) : i18ncp("@option:combobox", "After %1 second", "After %1 seconds", n);
            }
            function translateMinutes(n) { return i18ncp("@option:combobox", "After %1 minute", "After %1 minutes", n); }

            valueRole: "seconds"
            textRole: "text"
            unitOfValueRole: DurationPromptDialog.Unit.Seconds
            durationPromptFromValue: 60
            presetOptions: [
                { seconds: 1 * 60, text: translateMinutes(1), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 2 * 60, text: translateMinutes(2), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 5 * 60, text: translateMinutes(5), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 10 * 60, text: translateMinutes(10), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 15 * 60, text: translateMinutes(15), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 30 * 60, text: translateMinutes(30), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: -1, text: i18nc("@option:combobox Choose a custom value outside the list of preset values", "Custom…") },
            ]
            customRequesterValue: -1
            configuredValue: root.profileSettings.autoSuspendIdleTimeoutSec
            configuredDisplayUnit: model[indexOfValue(configuredValue)]?.unit ?? DurationPromptDialog.Unit.Minutes

            onRegularValueActivated: {
                root.profileSettings.autoSuspendIdleTimeoutSec = currentValue;
            }
            onCustomDurationAccepted: {
                root.profileSettings.autoSuspendIdleTimeoutSec = valueToUnit(
                    customDuration.value, customDuration.unit, DurationPromptDialog.Unit.Seconds);
            }

            onConfiguredValueOptionMissing: {
                const unit = configuredValue % 60 === 0
                    ? customDuration?.unit ?? DurationPromptDialog.Unit.Minutes
                    : DurationPromptDialog.Unit.Seconds;

                customOptions = [{
                    seconds: configuredValue,
                    text: translateSeconds(configuredValue, unit),
                    unit: unit,
                }];
                customDuration = null;
            }

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "AutoSuspendIdleTimeoutSec"
                extraEnabledConditions: autoSuspendActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction
            }
        }
    }

    // EU Regulation 2023/826 requires automatic suspend after max. 20 minutes and a warning
    // if the user disables auto suspend.
    Kirigami.InlineMessage {
        id: suspendComplianceWarning
        Layout.fillWidth: true
        Kirigami.FormData.isSection: true
        type: Kirigami.MessageType.Warning
        visible: text !== ""

        function showIfNeeded() {
            if (root.profileSettings.autoSuspendAction === PD.PowerDevil.PowerButtonAction.NoAction) {
                text = i18nc("@info:status", "Disabling automatic suspend will result in higher energy consumption.");
            } else if (root.profileSettings.autoSuspendIdleTimeoutSec > 20 * 60) {
                text = i18nc("@info:status A long duration until suspend/shutdown", "A long duration will result in higher energy consumption.");
            } else {
                text = "";
            }
        }
    }

    Kirigami.InlineMessage {
        Kirigami.FormData.isSection: true
        visible: powerButtonActionCombo.visible && !powerButtonActionCombo.isConfiguredValueSupported
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The action you had previously configured for when the power button is pressed is now unsupported on your system. Please select a different one.")
    }

    ComboBoxWithIcon {
        id: powerButtonActionCombo
        Kirigami.FormData.label: i18nc(
            "@label:combobox Suspend action such as sleep/hibernate to perform when the power button is pressed",
            "When &power button pressed:"
        )
        Accessible.name: i18nc("@accessible:name:combobox", "Action to perform when the power button is pressed")

        visible: kcm.supportedActions["HandleButtonEvents"] === true && kcm.isPowerButtonPresent
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: kcm.powerButtonActionModel
        textRole: "name"
        valueRole: "value"

        readonly property bool isConfiguredValueSupported: currentValue === root.profileSettings.powerButtonAction

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "PowerButtonAction"
        }
        Component.onCompleted: {
            // indexOfValue() is invalid before onCompleted, so wait until here to bind currentIndex.
            currentIndex = Qt.binding(() => indexOfValue(root.profileSettings.powerButtonAction));
        }
        onActivated: {
            root.profileSettings.powerButtonAction = currentValue;
        }
    }

    Kirigami.InlineMessage {
        Kirigami.FormData.isSection: true
        visible: lidActionCombo.visible && !lidActionCombo.isConfiguredValueSupported
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The action you had previously configured for when the lid is closed is now unsupported on your system. Please select a different one.")
    }

    ComboBoxWithIcon {
        id: lidActionCombo
        Kirigami.FormData.label: i18nc(
            "@label:combobox Suspend action such as sleep/hibernate to perform when the power button is pressed",
            "When laptop &lid closed:"
        )
        Accessible.name: i18nc("@accessible:name:combobox", "Action to perform when the laptop lid is closed")

        visible: kcm.supportedActions["HandleButtonEvents"] === true && kcm.isLidPresent
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: kcm.lidActionModel
        textRole: "name"
        valueRole: "value"

        readonly property bool isConfiguredValueSupported: currentValue === root.profileSettings.lidAction

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "LidAction"
        }
        Component.onCompleted: {
            // indexOfValue() is invalid before onCompleted, so wait until here to bind currentIndex.
            currentIndex = Qt.binding(() => indexOfValue(root.profileSettings.lidAction));
        }
        onActivated: {
            root.profileSettings.lidAction = currentValue;
        }
    }

    QQC2.CheckBox {
        id: triggersLidActionWhenExternalMonitorPresentCheck
        text: i18nc(
            "@text:checkbox Trigger laptop lid action even when an external monitor is connected",
            "Even when an external monitor is connected"
        )
        Accessible.name: i18n("Perform laptop lid action even when an external monitor is connected")

        visible: lidActionCombo.visible

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "InhibitLidActionWhenExternalMonitorPresent"
            extraEnabledConditions: lidActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction
        }
        checked: !root.profileSettings.inhibitLidActionWhenExternalMonitorPresent
        onToggled: { root.profileSettings.inhibitLidActionWhenExternalMonitorPresent = !checked; }
    }

    Kirigami.InlineMessage {
        Kirigami.FormData.isSection: true
        visible: sleepModeCombo.visible && sleepModeCombo.enabled && !sleepModeCombo.isConfiguredValueSupported
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The sleep mode you had previously configured is now unsupported on your system. Please select a different one.")
    }

    QQC2.ComboBox {
        id: sleepModeCombo
        Kirigami.FormData.label: i18nc(
            "@label:combobox Sleep mode selection - suspend to memory, disk or both",
            "When sleeping, enter:"
        )
        Accessible.name: i18nc("@accessible:name:combobox", "When sleeping, enter this power-save mode")

        visible: count > 1 && (kcm.supportedActions["SuspendSession"] === true || kcm.supportedActions["HandleButtonEvents"] === true)
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: kcm.sleepModeModel
        textRole: "name"
        valueRole: "value"

        delegate: Kirigami.SubtitleDelegate {
            required property string index

            // model roles from roleNames(), expose these in your QAbstractItemModel if you haven't already
            required property string name
            required property string subtext

            text: name
            subtitle: subtext
            icon.width: 0
            width: sleepModeCombo.popup.width
            highlighted: index === sleepModeCombo.currentIndex
        }

        readonly property bool isConfiguredValueSupported: currentValue === root.profileSettings.sleepMode

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "SleepMode"
            extraEnabledConditions: (
                autoSuspendActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
                || powerButtonActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
                || lidActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
            )
        }
        Component.onCompleted: {
            // indexOfValue() is invalid before onCompleted, so wait until here to bind currentIndex.
            currentIndex = Qt.binding(() => indexOfValue(root.profileSettings.sleepMode));
        }
        onActivated: {
            root.profileSettings.sleepMode = currentValue;
        }
    }

    //
    // Display and Brightness

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Display and Brightness")
        visible: (
            kcm.supportedActions["DimDisplay"] === true
            || kcm.supportedActions["DPMSControl"] === true
            || kcm.supportedActions["KeyboardBrightnessControl"] === true
            || kcm.supportedActions["ScreenBrightnessControl"] === true
        )
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider Brightness level", "Change scr&een brightness:")
        Kirigami.FormData.buddyFor: displayBrightnessCheck

        visible: kcm.supportedActions["ScreenBrightnessControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: displayBrightnessCheck

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "UseProfileSpecificDisplayBrightness"
            }
            checked: root.profileSettings.useProfileSpecificDisplayBrightness
            onToggled: { root.profileSettings.useProfileSpecificDisplayBrightness = checked; }
        }
        QQC2.Slider {
            id: displayBrightnessSlider
            Layout.fillWidth: true
            from: 1
            to: 100
            stepSize: 1

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "DisplayBrightness"
                extraEnabledConditions: displayBrightnessCheck.checked
            }
            value: root.profileSettings.displayBrightness
            onMoved: { root.profileSettings.displayBrightness = value; }
        }
        QQC2.Label {
            enabled: displayBrightnessCheck.checked
            text: root.formatPercentageText(displayBrightnessSlider.value)
            Layout.preferredWidth: displayBrightnessPercentageMetrics.width
        }
        TextMetrics {
            id: displayBrightnessPercentageMetrics
            text: root.formatPercentageText(100)
        }
    }

    TimeDurationComboBox {
        id: dimDisplayIdleTimeoutCombo
        visible: kcm.supportedActions["DimDisplay"] === true
        Layout.fillWidth: true
        Kirigami.FormData.label: i18nc("@label:combobox Dim screen after X minutes", "Di&m automatically:")

        durationPromptLabel: i18nc("@label:spinbox Dim screen after X minutes", "Di&m automatically after:")
        durationPromptAcceptsUnits: [DurationPromptDialog.Unit.Seconds, DurationPromptDialog.Unit.Minutes]

        function translateSeconds(n, formatUnit = DurationPromptDialog.Unit.Seconds) {
            return formatUnit == DurationPromptDialog.Unit.Minutes
                ? translateMinutes(n / 60) : i18ncp("@option:combobox", "%1 second", "%1 seconds", n);
        }
        function translateMinutes(n) { return i18ncp("@option:combobox", "%1 minute", "%1 minutes", n); }

        valueRole: "seconds"
        textRole: "text"
        unitOfValueRole: DurationPromptDialog.Unit.Seconds
        durationPromptFromValue: 10
        presetOptions: [
            { seconds: -1, text: i18nc("@option:combobox Dim screen automatically", "Never") },
            { seconds: 30, text: translateSeconds(30), unit: DurationPromptDialog.Unit.Seconds },
            { seconds: 1 * 60, text: translateMinutes(1), unit: DurationPromptDialog.Unit.Minutes },
            { seconds: 2 * 60, text: translateMinutes(2), unit: DurationPromptDialog.Unit.Minutes },
            { seconds: 5 * 60, text: translateMinutes(5), unit: DurationPromptDialog.Unit.Minutes },
            { seconds: 10 * 60, text: translateMinutes(10), unit: DurationPromptDialog.Unit.Minutes },
            { seconds: 15 * 60, text: translateMinutes(15), unit: DurationPromptDialog.Unit.Minutes },
            { seconds: -2, text: i18nc("@option:combobox Choose a custom value outside the list of preset values", "Custom…") },
        ]
        customRequesterValue: -2
        configuredValue: root.profileSettings.dimDisplayWhenIdle ? root.profileSettings.dimDisplayIdleTimeoutSec : -1
        configuredDisplayUnit: model[indexOfValue(configuredValue)]?.unit ?? DurationPromptDialog.Unit.Minutes

        onRegularValueActivated: {
            root.profileSettings.dimDisplayIdleTimeoutSec = currentValue;
            root.profileSettings.dimDisplayWhenIdle = currentValue > 0;
        }
        onCustomDurationAccepted: {
            root.profileSettings.dimDisplayIdleTimeoutSec = valueToUnit(
                customDuration.value, customDuration.unit, DurationPromptDialog.Unit.Seconds);
            root.profileSettings.dimDisplayWhenIdle = customDuration.value > 0;
        }

        onConfiguredValueOptionMissing: {
            const unit = configuredValue % 60 === 0
                ? customDuration?.unit ?? DurationPromptDialog.Unit.Minutes
                : DurationPromptDialog.Unit.Seconds;

            customOptions = [{
                seconds: configuredValue,
                text: translateSeconds(configuredValue, unit),
                unit: unit,
            }];
            customDuration = null;
        }

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "DimDisplayIdleTimeoutSec"
        }
    }

    Kirigami.InlineMessage {
        Kirigami.FormData.isSection: true
        visible: (
            dimDisplayIdleTimeoutCombo.visible
            && root.profileSettings.dimDisplayWhenIdle && root.profileSettings.turnOffDisplayWhenIdle
            && root.profileSettings.dimDisplayIdleTimeoutSec >= root.profileSettings.turnOffDisplayIdleTimeoutSec
            && root.profileSettings.dimDisplayIdleTimeoutSec >= root.profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec
        )
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The screen will not be dimmed because it is configured to turn off sooner.")
    }

    RowLayout {
        id: turnOffDisplayRow
        Kirigami.FormData.label: i18nc("@label:combobox After X minutes", "&Turn off screen:")
        Kirigami.FormData.buddyFor: turnOffDisplayIdleTimeoutCombo

        visible: kcm.supportedActions["DPMSControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        TimeDurationComboBox {
            id: turnOffDisplayIdleTimeoutCombo
            Layout.fillWidth: true

            durationPromptAcceptsUnits: [DurationPromptDialog.Unit.Seconds, DurationPromptDialog.Unit.Minutes]
            durationPromptLabel: i18nc("@label:spinbox After X minutes", "Turn off screen after:")

            function translateSeconds(n, formatUnit = DurationPromptDialog.Unit.Seconds) {
                return formatUnit == DurationPromptDialog.Unit.Minutes
                    ? translateMinutes(n / 60) : i18ncp("@option:combobox Turn off screen (caution: watch for string length)", "%1 second", "%1 seconds", n);
            }
            function translateMinutes(n) { return i18ncp("@option:combobox Turn off screen (caution: watch for string length)", "%1 minute", "%1 minutes", n); }

            valueRole: "seconds"
            textRole: "text"
            unitOfValueRole: DurationPromptDialog.Unit.Seconds
            durationPromptFromValue: 30
            presetOptions: [
                { seconds: -1, text: i18nc("@option:combobox Turn off screen (caution: watch for string length)", "Never") },
                { seconds: 1 * 60, text: translateMinutes(1), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 2 * 60, text: translateMinutes(2), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 5 * 60, text: translateMinutes(5), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 10 * 60, text: translateMinutes(10), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 15 * 60, text: translateMinutes(15), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 30 * 60, text: translateMinutes(30), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: -2, text: i18nc("@option:combobox Choose a custom value outside the list of preset values (caution: watch for string length)", "Custom…") },
            ]
            customRequesterValue: -2
            configuredValue: root.profileSettings.turnOffDisplayWhenIdle ? root.profileSettings.turnOffDisplayIdleTimeoutSec : -1
            configuredDisplayUnit: model[indexOfValue(configuredValue)]?.unit ?? DurationPromptDialog.Unit.Minutes

            onRegularValueActivated: {
                root.profileSettings.turnOffDisplayIdleTimeoutSec = currentValue;
                root.profileSettings.turnOffDisplayWhenIdle = currentValue > 0;
            }
            onCustomDurationAccepted: {
                root.profileSettings.turnOffDisplayIdleTimeoutSec = valueToUnit(
                    customDuration.value, customDuration.unit, DurationPromptDialog.Unit.Seconds);
                root.profileSettings.turnOffDisplayWhenIdle = customDuration.value > 0;
            }

            onConfiguredValueOptionMissing: {
                const unit = configuredValue % 60 === 0
                    ? customDuration?.unit ?? DurationPromptDialog.Unit.Minutes
                    : DurationPromptDialog.Unit.Seconds;

                customOptions = [{
                    seconds: configuredValue,
                    text: translateSeconds(configuredValue, unit),
                    unit: unit,
                }];
                customDuration = null;
            }

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "TurnOffDisplayIdleTimeoutSec"
            }
        }

        TimeDurationComboBox {
            id: turnOffDisplayIdleTimeoutWhenLockedCombo

            durationPromptAcceptsUnits: [DurationPromptDialog.Unit.Seconds, DurationPromptDialog.Unit.Minutes]
            durationPromptLabel: i18nc("@label:spinbox After X minutes", "When locked, turn off screen after:")

            function translateSeconds(n, formatUnit = DurationPromptDialog.Unit.Seconds) {
                return formatUnit == DurationPromptDialog.Unit.Minutes
                    ? translateMinutes(n / 60) : i18ncp("@option:combobox Turn off screen (caution: watch for string length)", "When locked: %1 second", "When locked: %1 seconds", n);
            }
            function translateMinutes(n) { return i18ncp("@option:combobox Turn off screen (caution: watch for string length)", "When locked: %1 minute", "When locked: %1 minutes", n); }

            valueRole: "seconds"
            textRole: "text"
            unitOfValueRole: DurationPromptDialog.Unit.Seconds
            durationPromptFromValue: 10
            presetOptions: [
                { seconds: -2, text: i18nc("@option:combobox Turn off screen after X minutes regardless of lock screen (caution: watch for string length)", "When locked and unlocked")  },
                // -1 would be "Never", same as for the unlocked timeout, if we want that option
                { seconds: 0, text: i18nc("@option:combobox Turn off screen (caution: watch for string length)", "When locked: Immediately") },
                { seconds: 20, text: translateSeconds(20), unit: DurationPromptDialog.Unit.Seconds },
                { seconds: 1 * 60, text: translateMinutes(1), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 2 * 60, text: translateMinutes(2), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 5 * 60, text: translateMinutes(5), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: -3, text: i18nc("@option:combobox Choose a custom value outside the list of preset values", "Custom…") },
            ]
            customRequesterValue: -3
            configuredValue: root.profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec
            configuredDisplayUnit: model[indexOfValue(configuredValue)]?.unit ?? DurationPromptDialog.Unit.Seconds

            onRegularValueActivated: { root.profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec = currentValue; }
            onCustomDurationAccepted: {
                root.profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec = valueToUnit(
                    customDuration.value, customDuration.unit, DurationPromptDialog.Unit.Seconds);
            }

            onConfiguredValueOptionMissing: {
                const unit = configuredValue % 60 === 0
                    ? customDuration?.unit ?? DurationPromptDialog.Unit.Minutes
                    : DurationPromptDialog.Unit.Seconds;

                customOptions = [{
                    seconds: configuredValue,
                    text: translateSeconds(configuredValue, unit),
                    unit: unit,
                }];
                customDuration = null;
            }

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "TurnOffDisplayIdleTimeoutWhenLockedSec"
                extraEnabledConditions: root.profileSettings.turnOffDisplayWhenIdle
            }
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider Brightness level", "Change key&board brightness:")
        Kirigami.FormData.buddyFor: keyboardBrightnessCheck

        visible: kcm.supportedActions["KeyboardBrightnessControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: keyboardBrightnessCheck

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "UseProfileSpecificKeyboardBrightness"
            }
            checked: root.profileSettings.useProfileSpecificKeyboardBrightness
            onToggled: { root.profileSettings.useProfileSpecificKeyboardBrightness = checked; }
        }
        QQC2.Slider {
            id: keyboardBrightnessSlider
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1

            KCM.SettingStateBinding {
                configObject: root.profileSettings
                settingName: "KeyboardBrightness"
                extraEnabledConditions: keyboardBrightnessCheck.checked
            }
            value: root.profileSettings.keyboardBrightness
            onMoved: { root.profileSettings.keyboardBrightness = value; }
        }
        QQC2.Label {
            enabled: keyboardBrightnessCheck.checked
            text: root.formatPercentageText(keyboardBrightnessSlider.value)
            Layout.preferredWidth: keyboardBrightnessPercentageMetrics.width
        }
        TextMetrics {
            id: keyboardBrightnessPercentageMetrics
            text: root.formatPercentageText(100)
        }
    }

    //
    // Advanced customization options

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Other Settings")
        visible: kcm.supportedActions["RunScript"] === true || powerProfileCombo.visible
    }

    ComboBoxWithIcon {
        id: powerProfileCombo
        Kirigami.FormData.label: i18nc(
            "@label:combobox Power Save, Balanced or Performance profile - same options as in the Battery applet",
            "Switch to po&wer profile:"
        )
        Accessible.name: i18nc(
            "@accessible:name:combobox Power Save, Balanced or Performance profile - same options as in the Battery applet",
            "Switch to power profile"
        )

        visible: kcm.supportedActions["PowerProfile"] === true && count > 1
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: kcm.powerProfileModel
        textRole: "name"
        valueRole: "value"

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "PowerProfile"
        }
        Component.onCompleted: {
            // indexOfValue() is invalid before onCompleted, so wait until here to bind currentIndex.
            // Also observe count - PowerProfileModel has delayed initialization due to a D-Bus call.
            currentIndex = Qt.binding(() => count ? indexOfValue(root.profileSettings.powerProfile) : -1);
        }
        onActivated: {
            root.profileSettings.powerProfile = currentValue;
        }
    }

    QQC2.Button {
        id: addScriptCommandButton
        Kirigami.FormData.label: i18nc("@label:button", "Run command or script:")

        visible: (kcm.supportedActions["RunScript"] === true
            // If power states aren't switchable, only show the idleTimeoutCommand field
            // because profile load and unload fields are not useful, just confusing.
            && kcm.supportsBatteryProfiles
        )
        icon.name: "settings-configure"
        text: i18nc(
            "@text:button Determine what will trigger a command or script to run in this power state",
            "Choose run conditions"
        )
        Accessible.name: i18nc("@accessible:name:button", "Choose run conditions for command or script")
        Accessible.role: Accessible.ButtonMenu

        onClicked: {
            if (addScriptCommandMenu.opened) {
                addScriptCommandMenu.close(); // closePolicy: Popup.CloseOnPressOutside does not seem to work?
            } else {
                addScriptCommandMenu.open();
            }
        }

        QQC2.Menu {
            id: addScriptCommandMenu
            title: addScriptCommandButton.text
            y: addScriptCommandButton.height

            Kirigami.Action {
                id: profileLoadCommandEditAction
                text: i18nc(
                    "@text:action:menu Command or script to run for power state (On AC Power, On Battery, ...)",
                    "When entering \"%1\" state", root.profileLabel
                )
                checkable: true
                Component.onCompleted: {
                    profileLoadCommandEditAction.checked = root.profileSettings.profileLoadCommand !== "";
                }
                onToggled: {
                    if (checked) {
                        profileLoadCommandEdit.forceActiveFocus();
                    } else {
                        root.profileSettings.profileLoadCommand = "";
                    }
                }
            }
            Kirigami.Action {
                id: profileUnloadCommandEditAction
                text: i18nc(
                    "@text:action:menu Command or script to run for power state (On AC Power, On Battery, ...)",
                    "When exiting \"%1\" state", root.profileLabel
                )
                checkable: true
                Component.onCompleted: {
                    profileUnloadCommandEditAction.checked = root.profileSettings.profileUnloadCommand !== "";
                }
                onToggled: {
                    if (checked) {
                        profileUnloadCommandEdit.forceActiveFocus();
                    } else {
                        root.profileSettings.profileUnloadCommand = "";
                    }
                }
            }
            Kirigami.Action {
                id: idleTimeoutCommandEditAction
                text: i18nc(
                    "@text:action:menu Command or script to run",
                    "When inactive"
                )
                checkable: true
                Component.onCompleted: {
                    idleTimeoutCommandEditAction.checked = (root.profileSettings.idleTimeoutCommand !== ""
                        // Always show field, regardless of addScriptCommandButton.visible.
                        || (kcm.supportedActions["RunScript"] === true && !kcm.supportsBatteryProfiles));
                }
                onToggled: {
                    if (checked) {
                        idleTimeoutCommandEdit.forceActiveFocus();
                    } else {
                        root.profileSettings.idleTimeoutCommand = "";
                    }
                }
            }
        }
    }

    RunScriptEdit {
        id: profileLoadCommandEdit
        Kirigami.FormData.label: i18nc(
            "@label:textfield Command or script to run for power state (On AC Power, On Battery, ...)",
            "When e&ntering \"%1\" state:", root.profileLabel
        )
        Accessible.name: i18nc(
            "@label:textfield for power state (On AC Power, On Battery, ...)",
            "Command or script when entering \"%1\" state", root.profileLabel
        )
        visible: profileLoadCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "ProfileLoadCommand"
        }
        command: root.profileSettings.profileLoadCommand
        onCommandChanged: {
            root.profileSettings.profileLoadCommand = command;
        }
        function resetToProfileSettings() {
            command = root.profileSettings.profileLoadCommand;
            profileLoadCommandEditAction.checked |= root.profileSettings.profileLoadCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { profileLoadCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: root.profileSettings
            function onProfileLoadCommandChanged() { profileLoadCommandEdit.resetToProfileSettings(); }
        }
    }

    RunScriptEdit {
        id: profileUnloadCommandEdit
        Kirigami.FormData.label: i18nc(
            "@label:textfield Command or script to run for power state (On AC Power, On Battery, ...)",
            "When e&xiting \"%1\" state:", root.profileLabel
        )
        Accessible.name: i18nc(
            "@label:textfield for power state (On AC Power, On Battery, ...)",
            "Command or script when exiting \"%1\" state", root.profileLabel
        )
        visible: profileUnloadCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "ProfileUnloadCommand"
        }
        command: root.profileSettings.profileUnloadCommand
        onCommandChanged: {
            root.profileSettings.profileUnloadCommand = command;
        }
        function resetToProfileSettings() {
            command = root.profileSettings.profileUnloadCommand;
            profileUnloadCommandEditAction.checked |= root.profileSettings.profileUnloadCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { profileUnloadCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: root.profileSettings
            function onProfileUnloadCommandChanged() { profileUnloadCommandEdit.resetToProfileSettings(); }
        }
    }

    TimeDurationComboBox {
        id: idleTimeoutCommandTimeDelayCombo
        Accessible.name: i18nc("@accessible:name:spinbox", "Duration of inactivity before the command or script runs")
        Layout.fillWidth: true
        visible: idleTimeoutCommandEdit.visible

        Kirigami.FormData.label: i18nc(
            "@label:textfield Command or script to run",
            "When i&nactive:"
        )
        
        durationPromptLabel: i18nc("@label:spinbox After X minutes", "Run command or script after:")
        durationPromptAcceptsUnits: [DurationPromptDialog.Unit.Seconds, DurationPromptDialog.Unit.Minutes]

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "RunScriptIdleTimeoutSec"
        }
        function translateSeconds(n, formatUnit = DurationPromptDialog.Unit.Seconds) {
            return formatUnit == DurationPromptDialog.Unit.Minutes
                ? translateMinutes(n / 60) : i18ncp("@option:combobox", "Run after %1 second", "Run after %1 seconds", n);
        }
        function translateMinutes(n) { return i18ncp("@option:combobox", "Run after %1 minute", "Run after %1 minutes", n); }

        valueRole: "seconds"
        textRole: "text"
        unitOfValueRole: DurationPromptDialog.Unit.Seconds
        durationPromptFromValue: 10
        presetOptions: [
                { seconds: 1 * 60, text: translateMinutes(1), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 2 * 60, text: translateMinutes(2), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 5 * 60, text: translateMinutes(5), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 10 * 60, text: translateMinutes(10), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 15 * 60, text: translateMinutes(15), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: 30 * 60, text: translateMinutes(30), unit: DurationPromptDialog.Unit.Minutes },
                { seconds: -1, text: i18nc("@option:combobox Choose a custom value outside the list of preset values", "Custom…") },
        ]
        customRequesterValue: -1
        configuredValue: root.profileSettings.runScriptIdleTimeoutSec
        configuredDisplayUnit: model[indexOfValue(configuredValue)]?.unit ?? DurationPromptDialog.Unit.Minutes

        onRegularValueActivated: { root.profileSettings.runScriptIdleTimeoutSec = currentValue; }
        onCustomDurationAccepted: {
            root.profileSettings.runScriptIdleTimeoutSec = valueToUnit(
                customDuration.value, customDuration.unit, DurationPromptDialog.Unit.Seconds);
        }

        onConfiguredValueOptionMissing: {
            const unit = configuredValue % 60 === 0
                ? customDuration?.unit ?? DurationPromptDialog.Unit.Minutes
                : DurationPromptDialog.Unit.Seconds;

            customOptions = [{
                seconds: configuredValue,
                text: translateSeconds(configuredValue, unit),
                unit: unit,
            }];
            customDuration = null;
        }
    }

    RunScriptEdit {
        id: idleTimeoutCommandEdit
        
        Accessible.name: i18nc("@accessible:name:textfield", "Command or script when inactive")

        visible: idleTimeoutCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: root.profileSettings
            settingName: "IdleTimeoutCommand"
        }
        command: root.profileSettings.idleTimeoutCommand
        onCommandChanged: {
            root.profileSettings.idleTimeoutCommand = command;
        }
        function resetToProfileSettings() {
            command = root.profileSettings.idleTimeoutCommand;
            idleTimeoutCommandEditAction.checked |= root.profileSettings.idleTimeoutCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { idleTimeoutCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: root.profileSettings
            function onIdleTimeoutCommandChanged() { idleTimeoutCommandEdit.resetToProfileSettings(); }
        }
    }
}
