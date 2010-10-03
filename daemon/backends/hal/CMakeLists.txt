include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

set(powerdevilhalbackend_SRCS
    halsuspendjob.cpp
    powerdevilhalbackend.cpp
)

kde4_add_plugin(powerdevilhalbackend ${powerdevilhalbackend_SRCS})

target_link_libraries(powerdevilhalbackend ${KDE4_KDECORE_LIBS} ${KDE4_SOLID_LIBS} powerdevilcore)

install(TARGETS powerdevilhalbackend DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES powerdevilhalbackend.desktop DESTINATION ${SERVICES_INSTALL_DIR})
