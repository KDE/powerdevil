set( kded_powerdevil_SRCS
    PowerDevilDaemon.cpp
    AbstractSystemPoller.cpp
    WidgetBasedPoller.cpp
    XSyncBasedPoller.cpp
    TimerBasedPoller.cpp
    PollSystemLoader.cpp
    PowerManagementConnector.cpp
)

kde4_add_kcfg_files(kded_powerdevil_SRCS ../PowerDevilSettings.kcfgc)

set(screensaver_xml "${CMAKE_CURRENT_SOURCE_DIR}/../../krunner/org.freedesktop.ScreenSaver.xml")

QT4_ADD_DBUS_INTERFACE(kded_powerdevil_SRCS ${screensaver_xml} screensaver_interface )
qt4_add_dbus_adaptor( kded_powerdevil_SRCS org.kde.PowerDevil.xml PowerDevilDaemon.h PowerDevilDaemon )

qt4_add_dbus_adaptor( kded_powerdevil_SRCS ${DBUS_INTERFACES_INSTALL_DIR}/org.kde.Solid.PowerManagement.xml PowerManagementConnector.h PowerManagementConnector )
qt4_add_dbus_adaptor( kded_powerdevil_SRCS ${DBUS_INTERFACES_INSTALL_DIR}/org.kde.Solid.PowerManagement.Inhibit.xml PowerManagementConnector.h PowerManagementConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor )

kde4_add_plugin( kded_powerdevil 
    ${kded_powerdevil_SRCS} 
)

target_link_libraries(kded_powerdevil 
    ${KDE4_KDECORE_LIBS} 
    ${KDE4_SOLID_LIBS}
    ${KDE4_KDEUI_LIBS}
    solidcontrol
    kworkspace
)

if(X11_Xss_LIB)
  target_link_libraries(kded_powerdevil ${X11_Xss_LIB})
endif(X11_Xss_LIB)

if(X11_Xext_LIB)
  target_link_libraries(kded_powerdevil ${X11_Xext_LIB})
endif(X11_Xext_LIB)

install( TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded )
install( FILES org.kde.PowerDevil.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )
