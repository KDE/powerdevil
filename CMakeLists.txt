
configure_file(config-powerdevil.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-powerdevil.h )

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR} 
)

add_subdirectory(daemon)
add_subdirectory(kcmodule)

install( FILES powerdevil.notifyrc DESTINATION ${DATA_INSTALL_DIR}/powerdevil )

