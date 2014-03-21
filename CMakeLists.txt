find_package(UDev) #upower/udev backend

find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "http://www.x.org"
                        TYPE OPTIONAL
                        PURPOSE "Required for building the X11 based workspace"
)
if(X11_FOUND)
  find_package(XCB REQUIRED COMPONENTS XCB XFIXES DAMAGE COMPOSITE SHAPE SYNC RENDER RANDR KEYSYMS IMAGE SHM XTEST)
  set_package_properties(XCB PROPERTIES TYPE REQUIRED)
  find_package(XCB COMPONENTS ICCCM)
  add_feature_info("XCB-ICCCM" XCB_ICCCM_FOUND "Required for building test applications for KWin")
endif()

include_directories (
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h)

set(POWERDEVIL_VERSION_MAJOR 4)
set(POWERDEVIL_VERSION_MINOR 90)
set(POWERDEVIL_VERSION_PATCH 1)
set(POWERDEVIL_VERSION ${POWERDEVIL_VERSION_MAJOR}.${POWERDEVIL_VERSION_MINOR}.${POWERDEVIL_VERSION_PATCH} )

set(GENERIC_LIB_SOVERSION ${POWERDEVIL_VERSION_MAJOR})
set(GENERIC_LIB_VERSION "${POWERDEVIL_VERSION_MAJOR}.${POWERDEVIL_VERSION_MINOR}.${POWERDEVIL_VERSION_PATCH}")

add_subdirectory(daemon)
add_subdirectory(kcmodule)

install( FILES powerdevil.notifyrc DESTINATION ${DATA_INSTALL_DIR}/powerdevil )
