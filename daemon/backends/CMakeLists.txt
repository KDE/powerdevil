########################## UPower Backend #####################################
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/upower
                    ${X11_INCLUDE_DIR}
                    ${X11_Xrandr_INCLUDE_PATH})

set(powerdevilupowerbackend_SRCS
    ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp
    upower/upowersuspendjob.cpp
    upower/login1suspendjob.cpp
    upower/powerdevilupowerbackend.cpp
    upower/xrandrbrightness.cpp
    upower/xrandrxcbhelper.cpp
    upower/ddcutilbrightness.cpp
)

set_source_files_properties(
${CMAKE_CURRENT_SOURCE_DIR}/upower/dbus/org.freedesktop.UPower.xml
${CMAKE_CURRENT_SOURCE_DIR}/upower/dbus/org.freedesktop.UPower.Device.xml
PROPERTIES NO_NAMESPACE TRUE)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/upower/dbus/org.freedesktop.UPower.xml
upower_interface)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/upower/dbus/org.freedesktop.UPower.Device.xml
upower_device_interface)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/upower/dbus/org.freedesktop.UPower.KbdBacklight.xml
upower_kbdbacklight_interface)

## backlight helper executable
add_executable(backlighthelper upower/backlighthelper.cpp ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp ${backlighthelper_mocs})
target_link_libraries(backlighthelper Qt5::Core KF5::Auth KF5::I18n)
install(TARGETS backlighthelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(backlighthelper org.kde.powerdevil.backlighthelper root)
kauth_install_actions(org.kde.powerdevil.backlighthelper ${CMAKE_CURRENT_SOURCE_DIR}/upower/backlight_helper_actions.actions)

## discrete gpu helper executable
add_executable(discretegpuhelper upower/discretegpuhelper.cpp ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp ${discretegpuhelper_mocs})
target_link_libraries(discretegpuhelper Qt5::Core KF5::Auth)
install(TARGETS discretegpuhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(discretegpuhelper org.kde.powerdevil.discretegpuhelper root)
kauth_install_actions(org.kde.powerdevil.discretegpuhelper ${CMAKE_CURRENT_SOURCE_DIR}/upower/discretegpu_helper_actions.actions)

add_library(powerdevilupowerbackend ${powerdevilupowerbackend_SRCS})
set_target_properties(powerdevilupowerbackend PROPERTIES PREFIX "")
target_link_libraries(powerdevilupowerbackend
    Qt5::Widgets
    KF5::Auth
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    ${X11_LIBRARIES}
    ${X11_Xrandr_LIB}
    ${XCB_XCB_LIBRARY}
    ${XCB_RANDR_LIBRARY}
    powerdevilcore
    ddcutil
)

install(TARGETS powerdevilupowerbackend DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/powerdevil)
