function(add_powerdevil_bundled_action _name)
    # Build the plugin
    kcoreaddons_add_plugin(powerdevil_${_name}action SOURCES ${_name}.cpp INSTALL_NAMESPACE "powerdevil/action")
    target_link_libraries(powerdevil_${_name}action
                        powerdevil_log_static
                        powerdevilcore
                        powerdevilui
                        Qt::Widgets
                        KF5::CoreAddons
                        KF5::I18n
                        KF5::ConfigGui
                        KF5::XmlGui
                        ${ARGN})
    # Build the config module
    set(actionconfig_SRCS ${_name}config.cpp)
    add_library(powerdevil${_name}action_config MODULE
                    ${actionconfig_SRCS})
    target_link_libraries(powerdevil${_name}action_config
                        powerdevilcore
                        powerdevilui
                        Qt::Widgets
                        KF5::CoreAddons
                        KF5::I18n
                        KF5::ConfigGui
                        KF5::XmlGui
                        ${ARGN})
    install(TARGETS powerdevil${_name}action_config DESTINATION ${KDE_INSTALL_PLUGINDIR})
endfunction(add_powerdevil_bundled_action _name)

function(target_dbus_files _target)
    set(dbus_SRCS)
    qt_add_dbus_adaptor(dbus_SRCS ${ARGN})
    target_sources(${_target} PRIVATE ${dbus_SRCS})
endfunction()

add_powerdevil_bundled_action(brightnesscontrol KF5::GlobalAccel)
target_dbus_files(powerdevil_brightnesscontrolaction  org.kde.Solid.PowerManagement.Actions.BrightnessControl.xml
                     brightnesscontrol.h PowerDevil::BundledActions::BrightnessControl)

add_powerdevil_bundled_action(keyboardbrightnesscontrol)
target_dbus_files(powerdevil_keyboardbrightnesscontrolaction org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl.xml
                     keyboardbrightnesscontrol.h PowerDevil::BundledActions::KeyboardBrightnessControl)

add_powerdevil_bundled_action(dimdisplay)
add_powerdevil_bundled_action(runscript KF5::KIOCore KF5::KIOWidgets)

add_powerdevil_bundled_action(suspendsession KF5::KIOCore KF5::KIOWidgets)
target_dbus_files(powerdevil_suspendsessionaction org.kde.Solid.PowerManagement.Actions.SuspendSession.xml
                     suspendsession.h PowerDevil::BundledActions::SuspendSession)

add_powerdevil_bundled_action(dpms)
target_link_libraries(powerdevil_dpmsaction KF5::ScreenDpms)

set(handlebuttons_SRCS handlebuttonevents.cpp)
qt_add_dbus_adaptor(handlebuttons_SRCS org.kde.Solid.PowerManagement.Actions.HandleButtonEvents.xml handlebuttonevents.h PowerDevil::BundledActions::HandleButtonEvents)

add_powerdevil_bundled_action(powerprofile)
target_dbus_files(powerdevil_powerprofileaction org.kde.Solid.PowerManagement.Actions.PowerProfile.xml powerprofile.h PowerDevil::BundledActions::PowerProfile)
set(powerprofile_dbus_SRCS)
qt_add_dbus_interface(powerprofile_dbus_SRCS org.freedesktop.DBus.Properties.xml properties_interface)
qt_add_dbus_interface(powerprofile_dbus_SRCS net.hadess.PowerProfiles.xml power_profiles_interface)
target_sources(powerdevil_powerprofileaction PRIVATE ${powerprofile_dbus_SRCS} ${handlebuttons_SRCS})

if(HAVE_WIRELESS_SUPPORT)
    add_powerdevil_bundled_action(wirelesspowersaving KF5::NetworkManagerQt KF5::BluezQt)
    target_dbus_files(powerdevil_wirelesspowersavingaction org.kde.Solid.PowerManagement.Actions.WirelessPowerSaving.xml
                         actions/bundled/wirelesspowersaving.h PowerDevil::BundledActions::WirelessPowerSaving)
endif()

kcoreaddons_add_plugin(powerdevil_handlebuttoneventsaction SOURCES powerdevilhandlebuttonevents_plugin.cpp ${handlebuttons_SRCS} INSTALL_NAMESPACE "powerdevil/action")
target_link_libraries(powerdevil_handlebuttoneventsaction PRIVATE powerdevilcore)

set(actionconfig_SRCS handlebuttoneventsconfig.cpp)
qt_add_dbus_interface(actionconfig_SRCS
    ${CMAKE_SOURCE_DIR}/daemon/backends/upower/dbus/org.freedesktop.UPower.xml
    upower_interface)
add_library(powerdevilhandlebuttoneventsaction_config MODULE
                ${actionconfig_SRCS})
target_link_libraries(powerdevilhandlebuttoneventsaction_config
                    powerdevilcore
                    powerdevilui
                    Qt::Widgets
                    KF5::CoreAddons
                    KF5::I18n
                    KF5::ConfigGui
                    KF5::XmlGui
                    )
install(TARGETS powerdevilhandlebuttoneventsaction_config DESTINATION ${KDE_INSTALL_PLUGINDIR})
