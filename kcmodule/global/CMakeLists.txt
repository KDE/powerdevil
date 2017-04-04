add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilglobalconfig\")

set( kcm_powerdevil_global_SRCS
     GeneralPage.cpp
)

ki18n_wrap_ui(kcm_powerdevil_global_SRCS
    generalPage.ui)

kconfig_add_kcfg_files(kcm_powerdevil_global_SRCS ../../PowerDevilSettings.kcfgc)

add_library(kcm_powerdevilglobalconfig MODULE ${kcm_powerdevil_global_SRCS})

target_link_libraries(kcm_powerdevilglobalconfig
     KF5::ConfigWidgets
     KF5::KIOWidgets
     KF5::NotifyConfig
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilglobalconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilglobalconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
