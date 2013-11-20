
set( powerdevil_config_common_private_SRCS
    actionconfigwidget.cpp
    actioneditwidget.cpp
    ErrorOverlay.cpp
)

add_library(powerdevilconfigcommonprivate SHARED ${powerdevil_config_common_private_SRCS})

target_link_libraries(powerdevilconfigcommonprivate
     
     powerdevilcore
     powerdevilui
)

set_target_properties(powerdevilconfigcommonprivate PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})

install( TARGETS powerdevilconfigcommonprivate ${INSTALL_TARGETS_DEFAULT_ARGS} )
