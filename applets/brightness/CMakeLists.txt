add_subdirectory(plugin)

ecm_qt_install_logging_categories(
    EXPORT APPLETS::BRIGHTNESS
    FILE applets/brightness.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

plasma_add_applet(org.kde.plasma.brightness
    QML_SOURCES
        BrightnessItem.qml
        CompactRepresentation.qml
        KeyboardColorItem.qml
        main.qml
        NightLightItem.qml
        PopupDialog.qml
    GENERATE_APPLET_CLASS
)

add_dependencies(org.kde.plasma.brightness brightnesscontrolplugin)
