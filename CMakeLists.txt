project(PowerDevil)

CMAKE_MINIMUM_REQUIRED(VERSION 2.4.0)
#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(KDE4 REQUIRED)

#configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include (MacroLibrary)
include (FindPackageHandleStandardArgs)
include (KDE4Defaults)

add_definitions (
    ${QT_DEFINITIONS} 
    ${QT_QTDBUS_DEFINITIONS} 
    ${KDE4_DEFINITIONS}
)

include_directories (
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 
    ${KDE4_INCLUDES} 
)

add_subdirectory(daemon)
add_subdirectory(kcmodule)
add_subdirectory(plasma)