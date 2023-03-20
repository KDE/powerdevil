add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilglobalconfig\")

set( kcm_powerdevil_global_SRCS
     GeneralPage.cpp
)

ki18n_wrap_ui(kcm_powerdevil_global_SRCS
    generalPage.ui)

kconfig_add_kcfg_files(kcm_powerdevil_global_SRCS ../../PowerDevilSettings.kcfgc)

kcoreaddons_add_plugin(kcm_powerdevilglobalconfig SOURCES ${kcm_powerdevil_global_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")
kcmutils_generate_desktop_file(kcm_powerdevilglobalconfig)

target_link_libraries(kcm_powerdevilglobalconfig
     KF6::AuthCore
     KF6::KCMUtils
     KF6::KIOWidgets
     KF6::NotifyConfig
     powerdevilconfigcommonprivate
)
