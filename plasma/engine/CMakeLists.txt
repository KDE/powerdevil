
set(powerdevil_engine_SRCS
    PowerDevilEngine.cpp
)

kde4_add_plugin(plasma_engine_powerdevil ${powerdevil_engine_SRCS})
target_link_libraries(plasma_engine_powerdevil ${KDE4_KDECORE_LIBS} ${PLASMA_LIBS})

install(TARGETS plasma_engine_powerdevil DESTINATION ${KDE4_LIB_DIR}/kde4)
install(FILES plasma-engine-powerdevil.desktop DESTINATION ${KDE4_LIB_DIR}/../share/kde4/services/)
