
set( kcm_powerdevil_profiles_SRCS
     EditPage.cpp
     ../../daemon/powerdevilprofilegenerator.cpp
)

kde4_add_ui_files(kcm_powerdevil_profiles_SRCS
    profileEditPage.ui)

kconfig_add_kcfg_files(kcm_powerdevil_profiles_SRCS ../../PowerDevilSettings.kcfgc)

add_library(kcm_powerdevilprofilesconfig MODULE ${kcm_powerdevil_profiles_SRCS})

target_link_libraries(kcm_powerdevilprofilesconfig
     KF5::KIOWidgets
     KF5::Solid
     KF5::KConfigCore
     powerdevilui
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilprofilesconfig  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES powerdevilprofilesconfig.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
