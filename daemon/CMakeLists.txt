include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(actions)

# Add bundled actions
set(powerdevil_bundled_actions_SRCS
    actions/bundled/suspendsession.cpp
    actions/bundled/disabledesktopeffects.cpp
    actions/bundled/brightnesscontrol.cpp
    actions/bundled/dimdisplay.cpp
    actions/bundled/runscript.cpp
    actions/bundled/handlebuttonevents.cpp
)

# target no.1 - powerdevil core library
set(powerdevilcore_SRCS
    powerdevilaction.cpp
    powerdevilactionpool.cpp
    powerdevilbackendinterface.cpp
    powerdevilcore.cpp
    powerdevilfdoconnector.cpp
    powerdevilpolicyagent.cpp
    powerdevilprofilegenerator.cpp
)

kde4_add_kcfg_files(powerdevilcore_SRCS ../PowerDevilSettings.kcfgc)

qt4_add_dbus_adaptor(powerdevilcore_SRCS org.kde.Solid.PowerManagement.xml powerdevilcore.h PowerDevil::Core)

set(screensaver_xml "${KDEBASE_WORKSPACE_SOURCE_DIR}/krunner/dbus/org.freedesktop.ScreenSaver.xml")
qt4_add_dbus_interface(powerdevilcore_SRCS ${screensaver_xml} screensaver_interface )

qt4_add_dbus_adaptor(powerdevilcore_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.freedesktop.PowerManagement.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementfdoadaptor PowerManagementFdoAdaptor)
qt4_add_dbus_adaptor(powerdevilcore_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.freedesktop.PowerManagement.Inhibit.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor)

kde4_add_library(powerdevilcore SHARED ${powerdevilcore_SRCS} ${powerdevil_bundled_actions_SRCS})

target_link_libraries(powerdevilcore
    ${KDE4_KDECORE_LIBS}
    ${KDE4_SOLID_LIBS}
    ${KDE4_KIDLETIME_LIBS}
    kworkspace
)

# target no.2 - powerdevil kded module
set(kded_powerdevil_SRCS
    kdedpowerdevil.cpp
)

kde4_add_plugin(kded_powerdevil ${kded_powerdevil_SRCS})

target_link_libraries(kded_powerdevil
    ${KDE4_KDECORE_LIBS}
    powerdevilcore
)

install(TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR})
install(TARGETS powerdevilcore DESTINATION ${LIB_INSTALL_DIR})

# target no.3 - powerdevil ui library
set(powerdevilui_SRCS
    powerdevilactionconfig.cpp
)

kde4_add_library(powerdevilui SHARED ${powerdevilui_SRCS})

target_link_libraries(powerdevilui
    ${KDE4_KDECORE_LIBS}
    ${QT_QTGUI_LIBRARY}
)

install(TARGETS powerdevilui DESTINATION ${LIB_INSTALL_DIR})
install(FILES powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded)

add_subdirectory(backends)

# 
# if(HAVE_DPMS)
#   include_directories( ${X11_dpms_INCLUDE_PATH}   )
# endif(HAVE_DPMS)
# 
# set( kded_powerdevil_SRCS
#     PowerDevilDaemon.cpp
#     SuspensionLockHandler.cpp
#     PowerManagementConnector.cpp
# )
# 
# kde4_add_kcfg_files(kded_powerdevil_SRCS ../PowerDevilSettings.kcfgc)
# 
# 
# #set(kscreensaver_xml  ${KDEBASE_WORKSPACE_SOURCE_DIR}/krunner/dbus/org.kde.screensaver.xml)
# set(ksmserver_xml  ${KDEBASE_WORKSPACE_SOURCE_DIR}/ksmserver/org.kde.KSMServerInterface.xml)
# 
# qt4_add_dbus_interface(kded_powerdevil_SRCS ${screensaver_xml} screensaver_interface )
# #qt4_add_dbus_interface(kded_powerdevil_SRCS ${kscreensaver_xml} kscreensaver_interface )
# qt4_add_dbus_interface(kded_powerdevil_SRCS ${ksmserver_xml} ksmserver_interface )
# qt4_add_dbus_adaptor( kded_powerdevil_SRCS org.kde.PowerDevil.xml PowerDevilDaemon.h PowerDevilDaemon )
# 
# qt4_add_dbus_adaptor( kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.freedesktop.PowerManagement.xml PowerManagementConnector.h PowerManagementConnector )
# qt4_add_dbus_adaptor( kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.freedesktop.PowerManagement.Inhibit.xml PowerManagementConnector.h PowerManagementConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor )
# 
# kde4_add_plugin( kded_powerdevil 
#     ${kded_powerdevil_SRCS} 
# )
# 
# target_link_libraries(kded_powerdevil 
#     ${KDE4_KDECORE_LIBS} 
#     ${KDE4_SOLID_LIBS}
#     ${KDE4_KDEUI_LIBS}
#     ${KDE4_KIDLETIME_LIBS}
#     solidcontrol
# )
# 
# if(HAVE_DPMS)
#   target_link_libraries(kded_powerdevil ${X11_LIBRARIES})
# endif(HAVE_DPMS)
# 
# install( TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR} )
# 
# install( FILES powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded )
# install( FILES org.kde.PowerDevil.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )
