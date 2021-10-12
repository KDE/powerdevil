add_definitions(-DTRANSLATION_DOMAIN=\"libpowerdevilcommonconfig\")

set(CMAKE_AUTOUIC TRUE)

set( powerdevil_config_common_private_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
    actioneditwidget.cpp
    ErrorOverlay.cpp
)

qt5_add_dbus_interface(powerdevil_config_common_private_SRCS
    ${CMAKE_SOURCE_DIR}/daemon/backends/upower/dbus/org.freedesktop.UPower.xml
    upower_interface)

add_library(powerdevilconfigcommonprivate SHARED ${powerdevil_config_common_private_SRCS})

# not exported, so just make the links public
target_link_libraries(powerdevilconfigcommonprivate
    PUBLIC
        powerdevilcore
        powerdevilui
        KF5::I18n
        KF5::Service
        KF5::KIOFileWidgets
)

set_target_properties(powerdevilconfigcommonprivate PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

install( TARGETS powerdevilconfigcommonprivate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
