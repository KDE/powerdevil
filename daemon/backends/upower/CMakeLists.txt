set(powerdevilupowerbackend_SRCS
    ${CMAKE_BINARY_DIR}/daemon/powerdevil_debug.cpp
    login1suspendjob.cpp
    powerdevilupowerbackend.cpp
    udevqtclient.cpp
    udevqtdevice.cpp
    ddcutilbrightness.cpp
)

set_source_files_properties(
    dbus/org.freedesktop.UPower.xml
    dbus/org.freedesktop.UPower.Device.xml
    PROPERTIES NO_NAMESPACE TRUE
)

qt_add_dbus_interface(powerdevilupowerbackend_SRCS
    dbus/org.freedesktop.UPower.xml
    upower_interface
)

qt_add_dbus_interface(powerdevilupowerbackend_SRCS
    dbus/org.freedesktop.UPower.Device.xml
    upower_device_interface
)

qt_add_dbus_interface(powerdevilupowerbackend_SRCS
    dbus/org.freedesktop.UPower.KbdBacklight.xml
    upower_kbdbacklight_interface
)

## backlight helper executable
add_executable(backlighthelper backlighthelper.cpp ${CMAKE_BINARY_DIR}/daemon/powerdevil_debug.cpp)
target_link_libraries(backlighthelper Qt::Core KF5::AuthCore KF5::I18n)
install(TARGETS backlighthelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(backlighthelper org.kde.powerdevil.backlighthelper root)
kauth_install_actions(org.kde.powerdevil.backlighthelper backlight_helper_actions.actions)

## discrete gpu helper executable
add_executable(discretegpuhelper discretegpuhelper.cpp ${CMAKE_BINARY_DIR}/daemon/powerdevil_debug.cpp)
target_link_libraries(discretegpuhelper Qt::Core KF5::AuthCore)
install(TARGETS discretegpuhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(discretegpuhelper org.kde.powerdevil.discretegpuhelper root)
kauth_install_actions(org.kde.powerdevil.discretegpuhelper discretegpu_helper_actions.actions)

add_library(powerdevilupowerbackend ${powerdevilupowerbackend_SRCS})
set_target_properties(powerdevilupowerbackend PROPERTIES PREFIX "")
target_link_libraries(powerdevilupowerbackend
    Qt::Widgets
    KF5::AuthCore
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    ${UDEV_LIBS}
    powerdevilcore
)
if(DDCUTIL_FOUND)
    target_link_libraries(powerdevilupowerbackend ${LIBDDCUTIL_LIBRARY})
endif()

install(TARGETS powerdevilupowerbackend DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf${QT_MAJOR_VERSION}/powerdevil)
