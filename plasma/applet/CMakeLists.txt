
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(plasma_applet_powerdevil_SRCS
  PowerDevilApplet.cpp
  AbstractView.cpp
  ErrorView.cpp
  IdleView.cpp
)

kde4_add_plugin(plasma_applet_powerdevil 
	${plasma_applet_powerdevil_SRCS}
)

target_link_libraries(plasma_applet_powerdevil 
	${PLASMA_LIBS} 
	${KDE4_KDEUI_LIBS}
	${KDE4_KIO_LIBS}
)

install(TARGETS plasma_applet_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR})
