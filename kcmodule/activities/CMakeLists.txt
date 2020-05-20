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

add_library(kcm_powerdevilactivitiesconfig MODULE ${kcm_powerdevil_activities_SRCS})

target_link_libraries(kcm_powerdevilactivitiesconfig
     KF5::Activities
     powerdevilconfigcommonprivate
)

install(TARGETS kcm_powerdevilactivitiesconfig  DESTINATION ${KDE_INSTALL_PLUGINDIR} )
install( FILES powerdevilactivitiesconfig.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )
