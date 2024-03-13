/*
 *  SPDX-FileCopyrightText: 2022, 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.powerdevil as PD

Kirigami.FormLayout {
    id: root

    required property string profileId
    readonly property var profileSettings: kcm.settings["profile" + profileId]

    function formatPercentageText(value) {
        return i18nc(
            "Percentage value example, used for formatting brightness levels in the power management settings",
            "10%"
        ).replace("10", value);
    }

    function maxTimeDelaySpinBoxImplicitWidth() {
        return Math.max(dimDisplayTimeDelay.implicitWidth, turnOffDisplayTimeDelay.implicitWidth, turnOffDisplayWhenLockedTimeDelay.implicitWidth, idleTimeoutCommandTimeDelay.implicitWidth);
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
        text: i18nc("@info:status", "The action you had previously configured for after a period of inactivity is now unsupported on your system. Please select a different one.")
    }

    RowLayout {
        id: autoSuspendActionRow
        Kirigami.FormData.label: i18nc(
            "@label:combobox Suspend action such as sleep/hibernate to perform when the system is idle",
            "A&fter a period of inactivity:"
        )
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

            currentIndex: indexOfValue(profileSettings.autoSuspendAction)
            readonly property bool isConfiguredValueSupported: currentValue === profileSettings.autoSuspendAction

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "AutoSuspendAction"
            }
            onActivated: {
                profileSettings.autoSuspendAction = currentValue;
            }
        }

        TimeDelaySpinBox {
            id: autoSuspendTimeDelay
            stepSize: 60
            from: 60
            to: 360 * 60

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "AutoSuspendIdleTimeoutSec"
                extraEnabledConditions: autoSuspendActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction
            }
            value: profileSettings.autoSuspendIdleTimeoutSec
            onValueModified: {
                profileSettings.autoSuspendIdleTimeoutSec = value;
                // In Qt 6.6, SpinBox breaks the value binding on keyboard input. Restore it again.
                value = Qt.binding(() => profileSettings.autoSuspendIdleTimeoutSec);
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

        currentIndex: indexOfValue(profileSettings.powerButtonAction)
        readonly property bool isConfiguredValueSupported: currentValue === profileSettings.powerButtonAction

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "PowerButtonAction"
        }
        onActivated: {
            profileSettings.powerButtonAction = currentValue;
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

        currentIndex: indexOfValue(profileSettings.lidAction)
        readonly property bool isConfiguredValueSupported: currentValue === profileSettings.lidAction

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "LidAction"
        }
        onActivated: {
            profileSettings.lidAction = currentValue;
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
            configObject: profileSettings
            settingName: "InhibitLidActionWhenExternalMonitorPresent"
            extraEnabledConditions: lidActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction
        }
        checked: !profileSettings.inhibitLidActionWhenExternalMonitorPresent
        onToggled: { profileSettings.inhibitLidActionWhenExternalMonitorPresent = !checked; }
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

        currentIndex: indexOfValue(profileSettings.sleepMode)
        readonly property bool isConfiguredValueSupported: currentValue === profileSettings.sleepMode

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "SleepMode"
            extraEnabledConditions: (
                autoSuspendActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
                || powerButtonActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
                || lidActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
            )
        }
        onActivated: {
            profileSettings.sleepMode = currentValue;
        }
    }

    //
    // Display and Brightness

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Display and Brightness")
        visible: (
            kcm.supportedActions["BrightnessControl"] === true
            || kcm.supportedActions["DimDisplay"] === true
            || kcm.supportedActions["DPMSControl"] === true
            || kcm.supportedActions["KeyboardBrightnessControl"] === true
        )
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider Brightness level", "Change scr&een brightness:")

        visible: kcm.supportedActions["BrightnessControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: displayBrightnessCheck

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "UseProfileSpecificDisplayBrightness"
            }
            checked: profileSettings.useProfileSpecificDisplayBrightness
            onToggled: { profileSettings.useProfileSpecificDisplayBrightness = checked; }
        }
        QQC2.Slider {
            id: displayBrightnessSlider
            Layout.fillWidth: true
            from: 1
            to: 100
            stepSize: 1

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "DisplayBrightness"
                extraEnabledConditions: displayBrightnessCheck.checked
            }
            value: profileSettings.displayBrightness
            onMoved: { profileSettings.displayBrightness = value; }
        }
        QQC2.Label {
            enabled: displayBrightnessCheck.checked
            text: formatPercentageText(displayBrightnessSlider.value)
            Layout.preferredWidth: displayBrightnessPercentageMetrics.width
        }
        TextMetrics {
            id: displayBrightnessPercentageMetrics
            text: formatPercentageText(100)
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:spinbox Dim screen after X minutes", "Di&m automatically:")

        visible: kcm.supportedActions["DimDisplay"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: dimDisplayCheck

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "DimDisplayWhenIdle"
            }
            checked: profileSettings.dimDisplayWhenIdle
            onToggled: { profileSettings.dimDisplayWhenIdle = checked; }
        }
        TimeDelaySpinBox {
            id: dimDisplayTimeDelay
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 60
            from: 60
            to: 360 * 60

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "DimDisplayIdleTimeoutSec"
                extraEnabledConditions: dimDisplayCheck.checked
            }
            value: profileSettings.dimDisplayIdleTimeoutSec
            onValueModified: {
                profileSettings.dimDisplayIdleTimeoutSec = value;
                // In Qt 6.6, SpinBox breaks the value binding on keyboard input. Restore it again.
                value = Qt.binding(() => profileSettings.dimDisplayIdleTimeoutSec);
            }
        }
    }

    RowLayout {
        id: turnOffDisplayRow
        Kirigami.FormData.label: i18nc("@label:spinbox After X minutes", "&Turn off screen:")

        visible: kcm.supportedActions["DPMSControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: turnOffDisplayCheck

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "TurnOffDisplayWhenIdle"
            }
            checked: profileSettings.turnOffDisplayWhenIdle
            onToggled: { profileSettings.turnOffDisplayWhenIdle = checked; }
        }
        TimeDelaySpinBox {
            id: turnOffDisplayTimeDelay
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 60
            from: 60
            to: 360 * 60

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "TurnOffDisplayIdleTimeoutSec"
                extraEnabledConditions: turnOffDisplayCheck.checked
            }
            value: profileSettings.turnOffDisplayIdleTimeoutSec
            onValueModified: {
                profileSettings.turnOffDisplayIdleTimeoutSec = value;
                // In Qt 6.6, SpinBox breaks the value binding on keyboard input. Restore it again.
                value = Qt.binding(() => profileSettings.turnOffDisplayIdleTimeoutSec);
            }
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:spinbox After X seconds", "When loc&ked, turn off screen:")

        visible: kcm.supportedActions["DPMSControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox { // dummy item to line up the two spinboxes
            enabled: false
            opacity: 0
        }
        TimeDelaySpinBox {
            id: turnOffDisplayWhenLockedTimeDelay
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 10
            from: 0
            to: turnOffDisplayTimeDelay.value

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "TurnOffDisplayIdleTimeoutWhenLockedSec"
                extraEnabledConditions: turnOffDisplayCheck.checked
            }
            value: profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec
            onValueModified: {
                profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec = value;
                // In Qt 6.6, SpinBox breaks the value binding on keyboard input. Restore it again.
                value = Qt.binding(() => profileSettings.turnOffDisplayIdleTimeoutWhenLockedSec);
            }
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider Brightness level", "Change key&board brightness:")

        visible: kcm.supportedActions["KeyboardBrightnessControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: keyboardBrightnessCheck

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "UseProfileSpecificKeyboardBrightness"
            }
            checked: profileSettings.useProfileSpecificKeyboardBrightness
            onToggled: { profileSettings.useProfileSpecificKeyboardBrightness = checked; }
        }
        QQC2.Slider {
            id: keyboardBrightnessSlider
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1

            KCM.SettingStateBinding {
                configObject: profileSettings
                settingName: "KeyboardBrightness"
                extraEnabledConditions: keyboardBrightnessCheck.checked
            }
            value: profileSettings.keyboardBrightness
            onMoved: { profileSettings.keyboardBrightness = value; }
        }
        QQC2.Label {
            enabled: keyboardBrightnessCheck.checked
            text: formatPercentageText(keyboardBrightnessSlider.value)
            Layout.preferredWidth: keyboardBrightnessPercentageMetrics.width
        }
        TextMetrics {
            id: keyboardBrightnessPercentageMetrics
            text: formatPercentageText(100)
        }
    }

    //
    // Advanced customization options

    Item {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nc("@title:group", "Other Settings")
        visible: kcm.supportedActions["RunScript"] === true || powerProfileCombo.visible
    }

    QQC2.ComboBox {
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

        /*private*/ property bool assignedInitialIndex: false

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "PowerProfile"
        }
        onCountChanged: { // PowerProfileModel has delayed initialization due to a D-Bus call
            if (count > 0 && !assignedInitialIndex) {
                currentIndex = Qt.binding(() => indexOfValue(profileSettings.powerProfile));
                assignedInitialIndex = true;
            }
        }
        onActivated: {
            profileSettings.powerProfile = currentValue;
        }
    }

    QQC2.Button {
        id: addScriptCommandButton
        Kirigami.FormData.label: i18nc("@label:button", "Run custom script:")
        visible: kcm.supportedActions["RunScript"] === true

        icon.name: "settings-configure"
        text: i18nc(
            "@text:button Determine what will trigger a script command to run in this power state",
            "Choose run conditions…"
        )
        Accessible.name: i18nc("@accessible:name:button", "Choose run conditions for script command")

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
                    "@text:action:menu Script command to execute",
                    "When e&ntering this power state"
                )
                checkable: true
                Component.onCompleted: {
                    profileLoadCommandEditAction.checked = profileSettings.profileLoadCommand !== "";
                }
                onToggled: {
                    if (!checked) {
                        profileSettings.profileLoadCommand = "";
                    }
                }
            }
            Kirigami.Action {
                id: profileUnloadCommandEditAction
                text: i18nc(
                    "@text:action:menu Script command to execute",
                    "When e&xiting this power state"
                )
                checkable: true
                Component.onCompleted: {
                    profileUnloadCommandEditAction.checked = profileSettings.profileUnloadCommand !== "";
                }
                onToggled: {
                    if (!checked) {
                        profileSettings.profileUnloadCommand = "";
                    }
                }
            }
            Kirigami.Action {
                id: idleTimeoutCommandEditAction
                text: i18nc(
                    "@text:action:menu Script command to execute",
                    "After a period of inacti&vity"
                )
                checkable: true
                Component.onCompleted: {
                    idleTimeoutCommandEditAction.checked = profileSettings.idleTimeoutCommand !== "";
                }
                onToggled: {
                    if (!checked) {
                        profileSettings.idleTimeoutCommand = "";
                    }
                }
            }
        }
    }

    RunScriptEdit {
        id: profileLoadCommandEdit
        Kirigami.FormData.label: i18nc(
            "@label:textfield Script command to execute",
            "When e&ntering this power state:"
        )
        Accessible.name: i18nc("@label:textfield", "Script command when entering this power state")

        visible: profileLoadCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "ProfileLoadCommand"
        }
        command: profileSettings.profileLoadCommand
        onCommandChanged: {
            profileSettings.profileLoadCommand = command;
        }
        function resetToProfileSettings() {
            command = profileSettings.profileLoadCommand;
            profileLoadCommandEditAction.checked |= profileSettings.profileLoadCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { profileLoadCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: profileSettings
            function onProfileLoadCommandChanged() { profileLoadCommandEdit.resetToProfileSettings(); }
        }
    }

    RunScriptEdit {
        id: profileUnloadCommandEdit
        Kirigami.FormData.label: i18nc(
            "@label:textfield Script command to execute",
            "When e&xiting this power state:"
        )
        Accessible.name: i18nc("@label:textfield", "Script command when exiting this power state")

        visible: profileUnloadCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "ProfileUnloadCommand"
        }
        command: profileSettings.profileUnloadCommand
        onCommandChanged: {
            profileSettings.profileUnloadCommand = command;
        }
        function resetToProfileSettings() {
            command = profileSettings.profileUnloadCommand;
            profileUnloadCommandEditAction.checked |= profileSettings.profileUnloadCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { profileUnloadCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: profileSettings
            function onProfileUnloadCommandChanged() { profileUnloadCommandEdit.resetToProfileSettings(); }
        }
    }

    RunScriptEdit {
        id: idleTimeoutCommandEdit
        Kirigami.FormData.label: i18nc(
            "@label:textfield Script command to execute",
            "After a period of inacti&vity:"
        )
        Accessible.name: i18nc("@@accessible:name:textfield", "Script command after a period of inactivity")

        visible: idleTimeoutCommandEditAction.checked
        Layout.fillWidth: true

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "IdleTimeoutCommand"
        }
        command: profileSettings.idleTimeoutCommand
        onCommandChanged: {
            profileSettings.idleTimeoutCommand = command;
        }
        function resetToProfileSettings() {
            command = profileSettings.idleTimeoutCommand;
            idleTimeoutCommandEditAction.checked |= profileSettings.idleTimeoutCommand !== "";
        }
        Connections {
            target: root
            function onProfileSettingsChanged() { idleTimeoutCommandEdit.resetToProfileSettings(); }
        }
        Connections {
            target: profileSettings
            function onIdleTimeoutCommandChanged() { idleTimeoutCommandEdit.resetToProfileSettings(); }
        }
    }

    TimeDelaySpinBox {
        id: idleTimeoutCommandTimeDelay
        Accessible.name: i18nc("@accessible:name:spinbox", "Period of inactivity until the script command executes")
        visible: idleTimeoutCommandEdit.visible
        Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

        stepSize: 60
        from: 60
        to: 360 * 60

        KCM.SettingStateBinding {
            configObject: profileSettings
            settingName: "RunScriptIdleTimeoutSec"
        }
        value: profileSettings.runScriptIdleTimeoutSec
        onValueModified: {
            profileSettings.runScriptIdleTimeoutSec = value;
            // In Qt 6.6, SpinBox breaks the value binding on keyboard input. Restore it again.
            value = Qt.binding(() => profileSettings.runScriptIdleTimeoutSec);
        }
    }
}
