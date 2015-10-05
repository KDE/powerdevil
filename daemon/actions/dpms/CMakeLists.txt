include_directories(${PowerDevil_SOURCE_DIR}/daemon
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

set(powerdevildpmsaction_SRCS
    ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp
    powerdevildpmsaction.cpp
    abstractdpmshelper.cpp
    xcbdpmshelper.cpp
    ${PowerDevil_SOURCE_DIR}/daemon/kwinkscreenhelpereffect.cpp
)

add_library(powerdevildpmsaction MODULE ${powerdevildpmsaction_SRCS})
target_link_libraries(powerdevildpmsaction
        Qt5::X11Extras
        XCB::XCB
        XCB::DPMS
        KF5::CoreAddons
        KF5::ConfigGui
        Qt5::Widgets
        powerdevilcore)
install(TARGETS powerdevildpmsaction DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES powerdevildpmsaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})

# Action config
set(dpmsconfig_SRCS powerdevildpmsactionconfig.cpp)
add_library(powerdevildpmsaction_config MODULE
                ${dpmsconfig_SRCS})
target_link_libraries(powerdevildpmsaction_config
                      KF5::CoreAddons
                      KF5::ConfigGui
                      KF5::I18n
                      Qt5::Widgets
                      powerdevilui)
install(TARGETS powerdevildpmsaction_config DESTINATION ${PLUGIN_INSTALL_DIR})
