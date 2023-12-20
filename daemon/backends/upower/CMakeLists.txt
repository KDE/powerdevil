set(powerdevilupowerbackend_SRCS
    ${CMAKE_BINARY_DIR}/daemon/powerdevil_debug.cpp
    powerdevilupowerbackend.cpp
)

add_library(powerdevilupowerbackend ${powerdevilupowerbackend_SRCS})

generate_export_header(powerdevilupowerbackend)

set_target_properties(powerdevilupowerbackend PROPERTIES PREFIX "")
target_link_libraries(powerdevilupowerbackend
    powerdevilcore
)

install(TARGETS powerdevilupowerbackend DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/powerdevil)
