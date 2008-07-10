set(kcm_powerdevil_SRCS PowerDevilKCM.cpp )

#kde4_add_ui_files(kcm_componentchooser_PART_SRCS browserconfig_ui.ui emailclientconfig_ui.ui componentchooser_ui.ui componentconfig_ui.ui) 

kde4_add_plugin(kcm_powerdevil ${kcm_powerdevil_SRCS})

target_link_libraries(kcm_powerdevil  
	${KDE4_KDECORE_LIBS}
	${KDE4_KDEUI_LIBRARY}
	${KDE4_KUTILS_LIBS} 
	${QT_QTGUI_LIBRARY}
)

install(TARGETS kcm_powerdevil  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES powerdevil.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

