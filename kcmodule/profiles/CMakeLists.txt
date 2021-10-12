add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilprofilesconfig\")

set( kcm_powerdevil_profiles_SRCS
     ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
     EditPage.cpp
     ${PowerDevil_SOURCE_DIR}/daemon/powerdevilprofilegenerator.cpp
)

kconfig_add_kcfg_files(kcm_powerdevil_profiles_SRCS ../../PowerDevilSettings.kcfgc)

add_library(kcm_powerdevilprofilesconfig MODULE ${kcm_powerdevil_profiles_SRCS})

target_link_libraries(kcm_powerdevilprofilesconfig
     KF5::KIOWidgets
     KF5::ConfigGui
     KF5::ConfigWidgets
     powerdevilui
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilprofilesconfig  DESTINATION ${KDE_INSTALL_PLUGINDIR} )
install( FILES powerdevilprofilesconfig.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
