# This files sets the needed sources for powerdevil's backend
# TODO 4.7: Compile only one backend instead of doing runtime checks


########################## UPower Backend #####################################
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backends/upower
                    ${CMAKE_CURRENT_BINARY_DIR}/backends/upower
                    ${X11_INCLUDE_DIR}
                    ${X11_Xrandr_INCLUDE_PATH})

set(powerdevilupowerbackend_SRCS
    backends/upower/upowersuspendjob.cpp
    backends/upower/powerdevilupowerbackend.cpp
    backends/upower/xrandrbrightness.cpp
)

set_source_files_properties(
  ${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.Device.xml
  PROPERTIES NO_NAMESPACE TRUE)

qt4_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.xml
upower_interface)

qt4_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.Device.xml
upower_device_interface)

qt4_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/backends/upower/dbus/org.freedesktop.UPower.KbdBacklight.xml
upower_kbdbacklight_interface)

set(powerdevilupowerbackend_LIBS ${X11_LIBRARIES} ${QT_QTGUI_LIBRARY} ${X11_Xrandr_LIB})

########################## HAL Backend #####################################

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backends/hal
                    ${CMAKE_CURRENT_BINARY_DIR}/backends/hal)

set(powerdevilhalbackend_SRCS
    backends/hal/halsuspendjob.cpp
    backends/hal/powerdevilhalbackend.cpp
)

set(powerdevilhalbackend_LIBS ${KDE4_SOLID_LIBS})

########################## Daemon variables ################################

set(POWERDEVIL_BACKEND_SRCS ${powerdevilupowerbackend_SRCS} ${powerdevilhalbackend_SRCS})
set(POWERDEVIL_BACKEND_LIBS ${powerdevilupowerbackend_LIBS} ${powerdevilhalbackend_LIBS})
