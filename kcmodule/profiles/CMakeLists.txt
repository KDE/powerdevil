add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilprofilesconfig\")

set( kcm_powerdevil_profiles_SRCS
     ${PowerDevil_SOURCE_DIR}/daemon/powerdevil_debug.cpp
     EditPage.cpp
     ${PowerDevil_SOURCE_DIR}/daemon/powerdevilprofilegenerator.cpp
)

ki18n_wrap_ui(kcm_powerdevil_profiles_SRCS
    profileEditPage.ui)

kconfig_add_kcfg_files(kcm_powerdevil_profiles_SRCS ../../PowerDevilSettings.kcfgc)

add_library(kcm_powerdevilprofilesconfig MODULE ${kcm_powerdevil_profiles_SRCS})

target_link_libraries(kcm_powerdevilprofilesconfig
     KF5::KIOWidgets
     KF5::Solid
     KF5::SolidPower
     KF5::ConfigGui
     KF5::ConfigWidgets
     powerdevilui
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilprofilesconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilprofilesconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
