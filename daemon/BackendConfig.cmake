# This files sets the needed sources for powerdevil's backend
# TODO 4.7: Compile only one backend instead of doing runtime checks


########################## UPower Backend #####################################
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backends/upower
                    ${X11_INCLUDE_DIR}
                    ${X11_Xrandr_INCLUDE_PATH})

set(powerdevilupowerbackend_SRCS
    ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp
    backends/upower/upowersuspendjob.cpp
    backends/upower/login1suspendjob.cpp
    backends/upower/powerdevilupowerbackend.cpp
    backends/upower/xrandrbrightness.cpp
    backends/upower/xrandrxcbhelper.cpp
    backends/upower/udevqtclient.cpp
    backends/upower/udevqtdevice.cpp
)

set_source_files_properties(
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.xml
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.Device.xml
PROPERTIES NO_NAMESPACE TRUE)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.xml
upower_interface)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.Device.xml
upower_device_interface)

qt5_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.KbdBacklight.xml
upower_kbdbacklight_interface)

set(powerdevilupowerbackend_LIBS ${X11_LIBRARIES} Qt5::Widgets KF5::Auth ${X11_Xrandr_LIB} ${UDEV_LIBS} ${XCB_XCB_LIBRARY}
${XCB_RANDR_LIBRARY})
## backlight helper executable
add_executable(backlighthelper backends/upower/backlighthelper.cpp ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp ${backlighthelper_mocs})
target_link_libraries(backlighthelper Qt5::Core KF5::Auth KF5::I18n)
install(TARGETS backlighthelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(backlighthelper org.kde.powerdevil.backlighthelper root)
kauth_install_actions(org.kde.powerdevil.backlighthelper ${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/backlight_helper_actions.actions)

########################## Daemon variables ################################

set(POWERDEVIL_BACKEND_SRCS ${powerdevilupowerbackend_SRCS})
set(POWERDEVIL_BACKEND_LIBS ${powerdevilupowerbackend_LIBS})
