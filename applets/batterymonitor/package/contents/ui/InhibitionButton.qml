

Item {
    width: blockButton.width
    height: blockButton.height

    PlasmaComponents3.Button {
        id: blockButton
        text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Unblock…")
        icon.name: "edit-delete-remove"
        onClicked: blockButtonMenu.open()
    }

    Menu {
        id: blockButtonMenu
        x: blockButton.x + blockButton.width - blockButtonMenu.width
        y: blockButton.y + blockButton.height

        MenuItem {
            text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Only this time")
            onTriggered: pmControl.blockInhibition(app, reason, false)
        }

        MenuItem {
            text: i18nc("@action:button Prevent an app from blocking automatic sleep and screen locking after inactivity", "Every time for this app and reason")
            onTriggered: pmControl.blockInhibition(app, reason, true)
        }
    }
}
