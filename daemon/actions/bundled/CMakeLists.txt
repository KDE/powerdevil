function(add_powerdevil_bundled_action _name)
    set(actionconfig_SRCS ${_name}config.cpp)
    add_library(powerdevil${_name}action_config MODULE
                    ${actionconfig_SRCS})
    target_link_libraries(powerdevil${_name}action_config
                        powerdevilui
                        Qt5::Widgets
                        KF5::CoreAddons
                        KF5::I18n
                        KF5::ConfigGui
                        KF5::XmlGui
                        ${ARGN})
    install(TARGETS powerdevil${_name}action_config DESTINATION ${PLUGIN_INSTALL_DIR})
    install(FILES powerdevil${_name}action.desktop DESTINATION ${SERVICES_INSTALL_DIR})
endfunction(add_powerdevil_bundled_action _name)

add_powerdevil_bundled_action(brightnesscontrol KF5::GlobalAccel)
add_powerdevil_bundled_action(keyboardbrightnesscontrol)
add_powerdevil_bundled_action(dimdisplay)
add_powerdevil_bundled_action(runscript KF5::KIOCore KF5::KIOWidgets)
add_powerdevil_bundled_action(suspendsession KF5::KIOCore KF5::KIOWidgets KF5::Solid KF5::SolidPower)


set(actionconfig_SRCS handlebuttoneventsconfig.cpp)
qt5_add_dbus_interface(actionconfig_SRCS
    ${CMAKE_SOURCE_DIR}/daemon/backends/upower/dbus/org.freedesktop.UPower.xml
    upower_interface)
add_library(powerdevilhandlebuttoneventsaction_config MODULE
                ${actionconfig_SRCS})
target_link_libraries(powerdevilhandlebuttoneventsaction_config
                    powerdevilui
                    Qt5::Widgets
                    KF5::CoreAddons
                    KF5::I18n
                    KF5::ConfigGui
                    KF5::Solid
                    KF5::SolidPower
                    KF5::XmlGui
                    KF5::GlobalAccel
                    KF5::IdleTime
                    )
install(TARGETS powerdevilhandlebuttoneventsaction_config DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES powerdevilhandlebuttoneventsaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})
