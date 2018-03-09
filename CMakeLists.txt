cmake_minimum_required(VERSION 3.0)

project(PowerDevil)
set(PROJECT_VERSION "5.12.80")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.9.0")
set(KF5_MIN_VERSION "5.42.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# require at least gcc 4.8
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "4.8")
        message(SEND_ERROR "Version ${CMAKE_CXX_COMPILER_VERSION} of the ${CMAKE_CXX_COMPILER_ID} C++ compiler is not supported. Please use version 4.8 or later.")
    endif()
endif()

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Widgets DBus X11Extras)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Activities Auth IdleTime Config DBusAddons Solid I18n GlobalAccel KIO NotifyConfig KDELibs4Support Wayland)
find_package(KF5Screen CONFIG REQUIRED)
find_package(LibKWorkspace CONFIG REQUIRED)

find_package(KF5BluezQt ${KF5_MIN_VERSION})
set_package_properties(KF5BluezQt
    PROPERTIES DESCRIPTION "Qt wrapper for BlueZ 5 DBus API"
    TYPE OPTIONAL
    PURPOSE "Support for wireless energy saving actions"
)
find_package(KF5NetworkManagerQt ${KF5_MIN_VERSION})
set_package_properties(KF5NetworkManagerQt
    PROPERTIES DESCRIPTION "Qt wrapper for NetworkManager API"
    TYPE OPTIONAL
    PURPOSE "Support for wireless energy saving actions"
)

set(HAVE_WIRELESS_SUPPORT FALSE)
if(KF5NetworkManagerQt_FOUND AND KF5BluezQt_FOUND)
    set(HAVE_WIRELESS_SUPPORT TRUE)
endif()
add_feature_info(
    "Wireless power saving"
    HAVE_WIRELESS_SUPPORT
    "Support turning off signal-transmitting devices to save energy"
)

find_package(UDev REQUIRED)

find_package(XCB REQUIRED COMPONENTS XCB RANDR DPMS)

option(WITH_DDCUTIL "DDCUtil library support" OFF)
if(WITH_DDCUTIL)
  find_package(DDCUtil REQUIRED)
  set_package_properties(DDCUtil
      PROPERTIES DESCRIPTION "DDCUtil library support"
      TYPE OPTIONAL
      PURPOSE "Set monitor settings over DDC/CI channel"
  )
else()
  add_feature_info("DDCUtil" "Off" "DDCUtil library support is disabled by default as recomemded by authors, add -DWITH_DDCUTIL=On to enable")
endif()

include_directories (
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

add_definitions(-DQT_NO_KEYWORDS)

add_definitions(-DQT_NO_KEYWORDS)

add_subdirectory(daemon)
add_subdirectory(kcmodule)
add_subdirectory(doc)

install( FILES powerdevil.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR} )

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
