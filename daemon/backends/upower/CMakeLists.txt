find_package(FindX11)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_BINARY_DIR} ${X11_INCLUDE_DIR} ${QT_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})

set(powerdevilupowerbackend_SRCS
    upowersuspendjob.cpp
    powerdevilupowerbackend.cpp
    xrandrbrightness.cpp
)

qt4_add_dbus_interface(powerdevilupowerbackend_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/dbus/org.freedesktop.UPower.xml
upower_interface)

kde4_add_plugin(powerdevilupowerbackend ${powerdevilupowerbackend_SRCS})

target_link_libraries(powerdevilupowerbackend ${KDE4_KDECORE_LIBS} ${KDE4_SOLID_LIBS} ${X11_LIBRARIES} ${QT_QTGUI_LIBRARY} ${X11_Xrandr_LIB} powerdevilcore)

install(TARGETS powerdevilupowerbackend DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES powerdevilupowerbackend.desktop DESTINATION ${SERVICES_INSTALL_DIR})
