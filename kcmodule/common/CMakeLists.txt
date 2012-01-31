
set( powerdevil_config_common_private_SRCS
    actionconfigwidget.cpp
    actioneditwidget.cpp
    ErrorOverlay.cpp
)

kde4_add_library(powerdevilconfigcommonprivate SHARED ${powerdevil_config_common_private_SRCS})

target_link_libraries(powerdevilconfigcommonprivate
     ${KDE4_KDECORE_LIBS}
     ${KDE4_KDEUI_LIBRARY}
     powerdevilcore
     powerdevilui
)

set_target_properties(powerdevilconfigcommonprivate PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION})

install( TARGETS powerdevilconfigcommonprivate ${INSTALL_TARGETS_DEFAULT_ARGS} )
