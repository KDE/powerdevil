
set( kcm_powerdevil_global_SRCS
     GeneralPage.cpp
)

kde4_add_ui_files(kcm_powerdevil_global_SRCS
    generalPage.ui)

kde4_add_kcfg_files(kcm_powerdevil_global_SRCS ../../PowerDevilSettings.kcfgc)

kde4_add_plugin(kcm_powerdevilglobalconfig ${kcm_powerdevil_global_SRCS})

target_link_libraries(kcm_powerdevilglobalconfig
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     ${KDE4_KIO_LIBS}
     ${KDE4_SOLID_LIBS}
     ${KDE4_KNOTIFYCONFIG_LIBRARY}
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilglobalconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilglobalconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
