set( kcm_powerdevil_SRCS 
     PowerDevilKCM.cpp
     ConfigWidget.cpp
     GeneralPage.cpp
     AssignmentPage.cpp
     EditPage.cpp
     CapabilitiesPage.cpp
)

kde4_add_ui_files(kcm_powerdevil_SRCS 
    dialog.ui
    generalPage.ui
    profileAssignmentPage.ui
    profileEditPage.ui
    capabilitiesPage.ui)

kde4_add_kcfg_files(kcm_powerdevil_SRCS ../PowerDevilSettings.kcfgc)

kde4_add_plugin(kcm_powerdevilconfig ${kcm_powerdevil_SRCS})

target_link_libraries(kcm_powerdevilconfig  
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     ${KDE4_KIO_LIBS}
     ${KDE4_SOLID_LIBS}
     solidcontrol
)

install(TARGETS kcm_powerdevilconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES powerdevilconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

