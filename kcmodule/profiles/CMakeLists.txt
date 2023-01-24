add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilprofilesconfig\")

set( kcm_powerdevil_profiles_SRCS
     ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
     EditPage.cpp EditPage.h
     ${PowerDevil_SOURCE_DIR}/daemon/powerdevilprofilegenerator.cpp ${PowerDevil_SOURCE_DIR}/daemon/powerdevilprofilegenerator.h
)

ki18n_wrap_ui(kcm_powerdevil_profiles_SRCS
    profileEditPage.ui)

kconfig_add_kcfg_files(kcm_powerdevil_profiles_SRCS ../../PowerDevilSettings.kcfgc)

kcoreaddons_add_plugin(kcm_powerdevilprofilesconfig SOURCES ${kcm_powerdevil_profiles_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")
kcmutils_generate_desktop_file(kcm_powerdevilprofilesconfig)

target_link_libraries(kcm_powerdevilprofilesconfig
     KF5::KIOWidgets
     KF5::ConfigGui
     KF5::ConfigWidgets
     KF5::Kirigami2
     powerdevilui
     powerdevilconfigcommonprivate
)
