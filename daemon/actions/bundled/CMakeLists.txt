function(add_powerdevil_bundled_action _name)
    set(actionconfig_SRCS ${_name}config.cpp)
    kde4_add_plugin(powerdevil${_name}action_config
                    ${actionconfig_SRCS})
    target_link_libraries(powerdevil${_name}action_config
                        ${KDE4_KDECORE_LIBS}
                        ${KDE4_KDEUI_LIBS}
                        powerdevilui
                        ${ARGN})
    install(TARGETS powerdevil${_name}action_config DESTINATION ${PLUGIN_INSTALL_DIR})
    install(FILES powerdevil${_name}action.desktop DESTINATION ${SERVICES_INSTALL_DIR})
endfunction(add_powerdevil_bundled_action _name)

add_powerdevil_bundled_action(brightnesscontrol)
add_powerdevil_bundled_action(dimdisplay)
add_powerdevil_bundled_action(runscript ${KDE4_KIO_LIBS})
add_powerdevil_bundled_action(suspendsession ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS})
add_powerdevil_bundled_action(handlebuttonevents ${KDE4_SOLID_LIBS})
