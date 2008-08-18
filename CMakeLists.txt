project(PowerDevil)

CMAKE_MINIMUM_REQUIRED(VERSION 2.4.0)
#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(KDE4 REQUIRED)
find_package(Plasma REQUIRED)

#configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include (MacroLibrary)
include (FindPackageHandleStandardArgs)
include (KDE4Defaults)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

add_definitions (
    ${QT_DEFINITIONS} 
    ${QT_QTDBUS_DEFINITIONS} 
    ${KDE4_DEFINITIONS}
)

include_directories (
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 
    ${KDE4_INCLUDES} 
    ${PLASMA_INCLUDES}
)

add_subdirectory(daemon)
add_subdirectory(kcmodule)
add_subdirectory(plasma)

install( FILES powerdevil.notifyrc DESTINATION ${DATA_INSTALL_DIR}/powerdevil )
