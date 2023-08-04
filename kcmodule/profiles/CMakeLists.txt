add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilprofilesconfig\")

set( kcm_powerdevil_profiles_SRCS
     ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
     EditPage.cpp
     ${PowerDevil_SOURCE_DIR}/daemon/powerdevilprofilegenerator.cpp
)

ki18n_wrap_ui(kcm_powerdevil_profiles_SRCS
    profileEditPage.ui)

kcoreaddons_add_plugin(kcm_powerdevilprofilesconfig SOURCES ${kcm_powerdevil_profiles_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")
kcmutils_generate_desktop_file(kcm_powerdevilprofilesconfig)

target_link_libraries(kcm_powerdevilprofilesconfig
     KF6::KIOWidgets
     KF6::ConfigGui
     KF6::KCMUtils
     KF6::Kirigami2
     powerdevilui
     powerdevilconfigcommonprivate
)
