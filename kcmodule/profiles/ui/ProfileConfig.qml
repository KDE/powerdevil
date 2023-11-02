/*
 *  SPDX-FileCopyrightText: 2022, 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.powerdevil as PD

Kirigami.FormLayout {
    id: root

    required property string profileId
    readonly property var profile: kcm.profileData[profileId]

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

            model: profile.autoSuspendActionModel
            textRole: "name"
            valueRole: "value"

            onActivated: {
                profile.settings.autoSuspendAction = currentValue;
            }
            Component.onCompleted: {
                autoSuspendActionCombo.currentIndex = autoSuspendActionCombo.indexOfValue(profile.settings.autoSuspendAction);
            }
            Connections {
                target: profile.settings
                function onAutoSuspendActionChanged() {
                    autoSuspendActionCombo.currentIndex = autoSuspendActionCombo.indexOfValue(profile.settings.autoSuspendAction);
                }
            }
        }

        TimeDelaySpinBox {
            enabled: autoSuspendActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction
            stepSize: 60
            from: 60
            to: 360 * 60
            value: profile.settings.autoSuspendIdleTimeoutSec
            onValueModified: { profile.settings.autoSuspendIdleTimeoutSec = value; }
        }
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

        model: profile.powerButtonActionModel
        textRole: "name"
        valueRole: "value"

        onActivated: {
            profile.settings.powerButtonAction = currentValue;
        }
        Component.onCompleted: {
            powerButtonActionCombo.currentIndex = powerButtonActionCombo.indexOfValue(profile.settings.powerButtonAction);
        }
        Connections {
            target: profile.settings
            function onPowerButtonActionChanged() {
                powerButtonActionCombo.currentIndex = powerButtonActionCombo.indexOfValue(profile.settings.powerButtonAction);
            }
        }
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

        model: profile.lidActionModel
        textRole: "name"
        valueRole: "value"

        onActivated: {
            profile.settings.lidAction = currentValue;
        }
        Component.onCompleted: {
            lidActionCombo.currentIndex = lidActionCombo.indexOfValue(profile.settings.lidAction);
        }
        Connections {
            target: profile.settings
            function onLidActionChanged() {
                lidActionCombo.currentIndex = lidActionCombo.indexOfValue(profile.settings.lidAction);
            }
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
        enabled: lidActionCombo.currentValue !== PD.PowerDevil.PowerButtonAction.NoAction

        checked: !profile.settings.inhibitLidActionWhenExternalMonitorPresent
        onToggled: { profile.settings.inhibitLidActionWhenExternalMonitorPresent = !checked; }
    }

    QQC2.ComboBox {
        id: sleepModeCombo
        Kirigami.FormData.label: i18nc(
            "@label:combobox Sleep mode selection - suspend to memory, disk or both",
            "When sleeping, enter:"
        )
        Accessible.name: i18nc("@accessible:name:combobox", "When sleeping, enter this power-save mode")

        visible: count > 1 && (kcm.supportedActions["SuspendSession"] === true || kcm.supportedActions["HandleButtonEvents"] === true)
        enabled: (
            autoSuspendActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
            || powerButtonActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
            || lidActionCombo.currentValue === PD.PowerDevil.PowerButtonAction.Sleep
        )
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: profile.sleepModeModel
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

        onActivated: {
            profile.settings.sleepMode = currentValue;
        }
        Component.onCompleted: {
            sleepModeCombo.currentIndex = sleepModeCombo.indexOfValue(profile.settings.sleepMode);
        }
        Connections {
            target: profile.settings
            function onSleepModeChanged() {
                sleepModeCombo.currentIndex = sleepModeCombo.indexOfValue(profile.settings.sleepMode);
            }
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
            checked: profile.settings.useProfileSpecificDisplayBrightness
            onToggled: { profile.settings.useProfileSpecificDisplayBrightness = checked; }
        }
        QQC2.Slider {
            id: displayBrightnessSlider
            Layout.fillWidth: true
            enabled: displayBrightnessCheck.checked
            from: 1
            to: 100
            stepSize: 1
            value: profile.settings.displayBrightness
            onMoved: { profile.settings.displayBrightness = value; }
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
            checked: profile.settings.dimDisplayWhenIdle
            onToggled: { profile.settings.dimDisplayWhenIdle = checked; }
        }
        TimeDelaySpinBox {
            id: dimDisplayTimeDelay
            enabled: dimDisplayCheck.checked
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 60
            from: 60
            to: 360 * 60
            value: profile.settings.dimDisplayIdleTimeoutSec
            onValueModified: { profile.settings.dimDisplayIdleTimeoutSec = value; }
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
            checked: profile.settings.turnOffDisplayWhenIdle
            onToggled: { profile.settings.turnOffDisplayWhenIdle = checked; }
        }
        TimeDelaySpinBox {
            id: turnOffDisplayTimeDelay
            enabled: turnOffDisplayCheck.checked
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 60
            from: 60
            to: 360 * 60
            value: profile.settings.turnOffDisplayIdleTimeoutSec
            onValueModified: { profile.settings.turnOffDisplayIdleTimeoutSec = value; }
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
            enabled: turnOffDisplayCheck.checked
            Layout.preferredWidth: maxTimeDelaySpinBoxImplicitWidth()

            stepSize: 10
            from: 0
            to: turnOffDisplayTimeDelay.value
            value: profile.settings.turnOffDisplayIdleTimeoutWhenLockedSec
            onValueModified: { profile.settings.turnOffDisplayIdleTimeoutWhenLockedSec = value; }
        }
    }

    RowLayout {
        Kirigami.FormData.label: i18nc("@label:slider Brightness level", "Change key&board brightness:")

        visible: kcm.supportedActions["KeyboardBrightnessControl"] === true
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        QQC2.CheckBox {
            id: keyboardBrightnessCheck
            checked: profile.settings.useProfileSpecificKeyboardBrightness
            onToggled: { profile.settings.useProfileSpecificKeyboardBrightness = checked; }
        }
        QQC2.Slider {
            id: keyboardBrightnessSlider
            Layout.fillWidth: true
            enabled: keyboardBrightnessCheck.checked
            from: 0
            to: 100
            stepSize: 1
            value: profile.settings.keyboardBrightness
            onMoved: { profile.settings.keyboardBrightness = value; }
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
        visible: kcm.supportedActions["RunScript"] === true || kcm.supportedActions["PowerProfile"] === true
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

        visible: kcm.supportedActions["PowerProfile"] === true
        Layout.fillWidth: true
        implicitContentWidthPolicy: QQC2.ComboBox.WidestTextWhenCompleted

        model: profile.powerProfileModel
        textRole: "name"
        valueRole: "value"

        /*private*/ property bool assignedInitialIndex: false

        onActivated: {
            profile.settings.powerProfile = currentValue;
        }
        onCountChanged: { // PowerProfileModel has delayed initialization due to a D-Bus call
            if (count > 0 && !assignedInitialIndex) {
                powerProfileCombo.currentIndex = powerProfileCombo.indexOfValue(profile.settings.powerProfile);
                assignedInitialIndex = true;
            }
        }
        Connections {
            target: profile.settings
            function onPowerProfileChanged() {
                powerProfileCombo.currentIndex = powerProfileCombo.indexOfValue(profile.settings.powerProfile);
            }
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
                    profileLoadCommandEditAction.checked = profile.settings.profileLoadCommand !== "";
                }
                onToggled: {
                    if (!profileLoadCommandEditAction.checked) {
                        profile.settings.profileLoadCommand = "";
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
                    profileUnloadCommandEditAction.checked = profile.settings.profileUnloadCommand !== "";
                }
                onToggled: {
                    if (!profileUnloadCommandEditAction.checked) {
                        profile.settings.profileUnloadCommand = "";
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
                    idleTimeoutCommandEditAction.checked = profile.settings.idleTimeoutCommand !== "";
                }
                onToggled: {
                    if (!idleTimeoutCommandEditAction.checked) {
                        profile.settings.idleTimeoutCommand = "";
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

        command: profile.settings.profileLoadCommand
        onCommandChanged: { profile.settings.profileLoadCommand = command; }
        Connections {
            target: profile.settings
            function onProfileLoadCommandChanged() {
                profileLoadCommandEdit.command = profile.settings.profileLoadCommand;
                profileLoadCommandEditAction.checked |= profile.settings.profileLoadCommand !== "";
            }
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

        command: profile.settings.profileUnloadCommand
        onCommandChanged: { profile.settings.profileUnloadCommand = command; }
        Connections {
            target: profile.settings
            function onProfileUnloadCommandChanged() {
                profileUnloadCommandEdit.command = profile.settings.profileUnloadCommand;
                profileUnloadCommandEditAction.checked |= profile.settings.profileUnloadCommand !== "";
            }
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

        command: profile.settings.idleTimeoutCommand
        onCommandChanged: { profile.settings.idleTimeoutCommand = command; }
        Connections {
            target: profile.settings
            function onIdleTimeoutCommandChanged() {
                idleTimeoutCommandEdit.command = profile.settings.idleTimeoutCommand;
                idleTimeoutCommandEditAction.checked |= profile.settings.idleTimeoutCommand !== "";
            }
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
        value: profile.settings.runScriptIdleTimeoutSec
        onValueModified: { profile.settings.runScriptIdleTimeoutSec = value; }
    }
}
