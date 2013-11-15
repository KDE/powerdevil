
set( kcm_powerdevil_global_SRCS
     GeneralPage.cpp
)

kde4_add_ui_files(kcm_powerdevil_global_SRCS
    generalPage.ui)

kde4_add_kcfg_files(kcm_powerdevil_global_SRCS ../../PowerDevilSettings.kcfgc)

add_library(kcm_powerdevilglobalconfig MODULE ${kcm_powerdevil_global_SRCS})

target_link_libraries(kcm_powerdevilglobalconfig
     
     ${KDE4_KDEUI_LIBRARY}
     KF5::KIOCore KF5::KIOWidgets
     KF5::Solid
     KF5::KNotifyConfig
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilglobalconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilglobalconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
