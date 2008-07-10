set( kded_powerdevil_SRCS
    PowerDevilDaemon.cpp 
)

kde4_add_kcfg_files(kded_powerdevil_SRCS GENERATE_MOC ../PowerDevilSettings.kcfgc)

kde4_add_plugin( kded_powerdevil 
    ${kded_powerdevil_SRCS} 
)

target_link_libraries(kded_powerdevil 
    ${KDE4_KDECORE_LIBS} 
    ${KDE4_SOLID_LIBS}
    ${KDE4_KIO_LIBRARY}
    solidcontrol
    ${X11_LIBRARIES}
)

install( TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kded_powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded )