add_subdirectory(plugin)

ecm_qt_install_logging_categories(
        EXPORT APPLETS::BATTERYMONITOR
        FILE applets/batterymonitor.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

plasma_add_applet(org.kde.plasma.battery
    QML_SOURCES
        BatteryItem.qml
        CompactRepresentation.qml
        InhibitionHint.qml
        InhibitionItem.qml
        main.qml
        PopupDialog.qml
        PowerProfileItem.qml
    RESOURCES
        main.xml
    GENERATE_APPLET_CLASS
)
