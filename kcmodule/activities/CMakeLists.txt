
set( kcm_powerdevil_activities_SRCS
     activitypage.cpp
     activitywidget.cpp
     ../common/ErrorOverlay.cpp
)

kde4_add_ui_files(kcm_powerdevil_activities_SRCS
    activityWidget.ui)

kde4_add_kcfg_files(kcm_powerdevil_activities_SRCS ../../PowerDevilSettings.kcfgc)

kde4_add_plugin(kcm_powerdevilactivitiesconfig ${kcm_powerdevil_activities_SRCS})

target_link_libraries(kcm_powerdevilactivitiesconfig
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     ${KDE4_SOLID_LIBS}
     ${KACTIVITIES_LIBRARY}
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilactivitiesconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilactivitiesconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
