project(PowerDevil)
set(PROJECT_VERSION "5.2.90")
set(PROJECT_VERSION_MAJOR 5)

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
set(QT_MIN_VERSION "5.2.0")

find_package(ECM 1.2.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Widgets DBus X11Extras)
find_package(KF5 REQUIRED COMPONENTS Auth IdleTime Config Solid I18n GlobalAccel KIO NotifyConfig KDELibs4Support)
find_package(LibKWorkspace CONFIG REQUIRED)
find_package(ScreenSaverDBusInterface CONFIG REQUIRED)

find_package(UDev) #upower/udev backend

find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "http://www.x.org"
                        TYPE OPTIONAL
                        PURPOSE "Required for building the X11 based workspace"
)
if(X11_FOUND)
  find_package(XCB REQUIRED COMPONENTS XCB RANDR)
  set_package_properties(XCB PROPERTIES TYPE REQUIRED)
endif()

include_directories (
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

set(HAVE_DPMS ${X11_dpms_FOUND})

configure_file(config-workspace.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-workspace.h)

add_subdirectory(daemon)
add_subdirectory(kcmodule)
add_subdirectory(doc)

install( FILES powerdevil.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR} )

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
