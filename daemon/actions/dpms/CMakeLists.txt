include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR}
                    ${X11_dpms_INCLUDE_PATH})

set(powerdevildpmsaction_SRCS powerdevildpmsaction.cpp)

add_library(powerdevildpmsaction MODULE ${powerdevildpmsaction_SRCS})
target_link_libraries(powerdevildpmsaction
        ${X11_LIBRARIES}
        KF5::KDE4Support
        Qt5::Widgets
        powerdevilcore)
install(TARGETS powerdevildpmsaction DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES powerdevildpmsaction.desktop DESTINATION ${SERVICES_INSTALL_DIR})

# Action config
set(dpmsconfig_SRCS powerdevildpmsactionconfig.cpp)
add_library(powerdevildpmsaction_config MODULE
                ${dpmsconfig_SRCS})
target_link_libraries(powerdevildpmsaction_config
                      KF5::KDE4Support
                      Qt5::Widgets
                      powerdevilui)
install(TARGETS powerdevildpmsaction_config DESTINATION ${PLUGIN_INSTALL_DIR})