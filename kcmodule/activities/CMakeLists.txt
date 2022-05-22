add_definitions(-DTRANSLATION_DOMAIN=\"powerdevilactivitiesconfig\")

set( kcm_powerdevil_activities_SRCS
     ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
     activitypage.cpp
     activitywidget.cpp
     ../common/ErrorOverlay.cpp
)

ki18n_wrap_ui(kcm_powerdevil_activities_SRCS
    activityWidget.ui)

kconfig_add_kcfg_files(kcm_powerdevil_activities_SRCS ../../PowerDevilSettings.kcfgc)

kcoreaddons_add_plugin(kcm_powerdevilactivitiesconfig SOURCES ${kcm_powerdevil_activities_SRCS} INSTALL_NAMESPACE "plasma/kcms/systemsettings_qwidgets")
kcoreaddons_desktop_to_json(kcm_powerdevilactivitiesconfig kcm_powerdevilactivitiesconfig.desktop)
target_link_libraries(kcm_powerdevilactivitiesconfig
     KF5::Activities
     powerdevilconfigcommonprivate
)

install(FILES kcm_powerdevilactivitiesconfig.desktop  DESTINATION  ${KDE_INSTALL_APPDIR})
