
set( kcm_powerdevil_profiles_SRCS
     EditPage.cpp
     actionconfigwidget.cpp
     ../../daemon/powerdevilprofilegenerator.cpp
     ../common/ErrorOverlay.cpp
)

kde4_add_ui_files(kcm_powerdevil_profiles_SRCS
    profileEditPage.ui)

kde4_add_kcfg_files(kcm_powerdevil_profiles_SRCS ../../PowerDevilSettings.kcfgc)

kde4_add_plugin(kcm_powerdevilprofilesconfig ${kcm_powerdevil_profiles_SRCS})

target_link_libraries(kcm_powerdevilprofilesconfig
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     ${KDE4_KIO_LIBS}
     ${KDE4_SOLID_LIBS}
     powerdevilui
)

install(TARGETS kcm_powerdevilprofilesconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilprofilesconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
