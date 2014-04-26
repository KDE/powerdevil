project(PowerDevil)

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
set(QT_MIN_VERSION "5.2.0")

find_package(ECM 0.0.8 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Widgets DBus X11Extras)
find_package(KF5 REQUIRED COMPONENTS Auth IdleTime Config Solid I18n GlobalAccel KIO WindowSystem Plasma NotifyConfig KDELibs4Support)
find_package(LibKWorkspace CONFIG)
find_package(ScreenSaverDBusInterface CONFIG)

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
set(POWERDEVIL_VERSION_MINOR 95)
set(POWERDEVIL_VERSION_PATCH 0)
set(POWERDEVIL_VERSION ${POWERDEVIL_VERSION_MAJOR}.${POWERDEVIL_VERSION_MINOR}.${POWERDEVIL_VERSION_PATCH} )

set(GENERIC_LIB_SOVERSION ${POWERDEVIL_VERSION_MAJOR})
set(GENERIC_LIB_VERSION "${POWERDEVIL_VERSION_MAJOR}.${POWERDEVIL_VERSION_MINOR}.${POWERDEVIL_VERSION_PATCH}")

add_subdirectory(daemon)
add_subdirectory(kcmodule)

install( FILES powerdevil.notifyrc DESTINATION ${DATA_INSTALL_DIR}/powerdevil )

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
