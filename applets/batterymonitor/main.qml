import QtQuick

import org.kde.plasma.plasmoid

PlasmoidItem {
    id: batterymonitor

    property bool v: Plasmoid.configuration.showPercentage
}
