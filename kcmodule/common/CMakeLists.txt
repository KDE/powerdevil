add_definitions(-DTRANSLATION_DOMAIN=\"libpowerdevilcommonconfig\")

set( powerdevil_config_common_private_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/../../daemon/powerdevil_debug.cpp
    actionconfigwidget.cpp
    actioneditwidget.cpp
    ErrorOverlay.cpp
)

add_library(powerdevilconfigcommonprivate SHARED ${powerdevil_config_common_private_SRCS})

# not exported, so just make the links public
target_link_libraries(powerdevilconfigcommonprivate
    PUBLIC
        powerdevilcore
        powerdevilui
        KF5::I18n
        KF5::Service
)

set_target_properties(powerdevilconfigcommonprivate PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

install( TARGETS powerdevilconfigcommonprivate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
