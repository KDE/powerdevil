
set(powerdevil_engine_SRCS
    PowerDevilEngine.cpp
)

kde4_add_plugin(plasma_engine_powerdevil ${powerdevil_engine_SRCS})
target_link_libraries(plasma_engine_powerdevil ${KDE4_KDECORE_LIBS} ${PLASMA_LIBS})

install(TARGETS plasma_engine_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-engine-powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR})
