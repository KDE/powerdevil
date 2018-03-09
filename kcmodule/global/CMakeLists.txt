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
     KF5::Solid
     KF5::NotifyConfig
     KF5::KDELibs4Support
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilglobalconfig  DESTINATION ${KDE_INSTALL_PLUGINDIR} )
install( FILES powerdevilglobalconfig.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
