set( kcm_powerdevil_SRCS 
     PowerDevilKCM.cpp
     ConfigWidget.cpp
)

kde4_add_ui_files(kcm_powerdevil_SRCS dialog.ui ) 

kde4_add_kcfg_files(kcm_powerdevil_SRCS ../PowerDevilSettings.kcfgc)

kde4_add_plugin(kcm_powerdevilconfig ${kcm_powerdevil_SRCS})

target_link_libraries(kcm_powerdevilconfig  
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     #${KDE4_KUTILS_LIBS} 
     ${KDE4_SOLID_LIBS}
     solidcontrol
     ${QT_QTGUI_LIBRARY}
)

install(TARGETS kcm_powerdevilconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES powerdevilconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

