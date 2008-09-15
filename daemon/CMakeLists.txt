set( kded_powerdevil_SRCS
    PowerDevilDaemon.cpp
    AbstractSystemPoller.cpp
    WidgetBasedPoller.cpp
    XSyncBasedPoller.cpp
    TimerBasedPoller.cpp
    PollSystemLoader.cpp 
)

kde4_add_kcfg_files(kded_powerdevil_SRCS ../PowerDevilSettings.kcfgc)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../krunner/org.freedesktop.ScreenSaver.xml")
  set(screensaver_xml "${CMAKE_CURRENT_SOURCE_DIR}/../../krunner/org.freedesktop.ScreenSaver.xml")
else(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../krunner/org.freedesktop.ScreenSaver.xml")
  #include(PkgConfigGetVar)
  #PKGCONFIG_GETVAR(dbus-1 prefix DBUS_PREFIX)
  #set(screensaver_xml ${DBUS_PREFIX}/share/dbus-1/interfaces/org.freedesktop.ScreenSaver.xml)
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../krunner/org.freedesktop.ScreenSaver.xml")

QT4_ADD_DBUS_INTERFACE(kded_powerdevil_SRCS ${screensaver_xml} screensaver_interface )
qt4_add_dbus_adaptor( kded_powerdevil_SRCS org.kde.PowerDevil.xml PowerDevilDaemon.h PowerDevilDaemon )

kde4_add_plugin( kded_powerdevil 
    ${kded_powerdevil_SRCS} 
)

target_link_libraries(kded_powerdevil 
    ${KDE4_KDECORE_LIBS} 
    ${KDE4_SOLID_LIBS}
    ${KDE4_KDEUI_LIBS}
    solidcontrol
    kworkspace
    ${X11_Xscreensaver_LIB}
    -lXext
)

install( TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded )
install( FILES org.kde.PowerDevil.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )
#INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/powerdevil.conf DESTINATION /etc/dbus-1/system.d)
