cmake_minimum_required(VERSION 3.16)

project(PowerDevil)
set(PROJECT_VERSION "5.22.0")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.82")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH}  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMConfiguredInstall)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Widgets DBus X11Extras)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Activities Auth IdleTime Config DBusAddons Solid I18n GlobalAccel KIO NotifyConfig Wayland DocTools Crash Notifications)
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

find_package(LibKWorkspace ${PROJECT_VERSION} REQUIRED)

find_package(UDev REQUIRED)

find_package(XCB REQUIRED COMPONENTS XCB RANDR DPMS)

find_package(Libcap)
set_package_properties(Libcap PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Needed for scheduled wakeup which can wake from suspend (CAP_WAKE_ALARM)"
)
set(HAVE_LIBCAP ${Libcap_FOUND})

option(HAVE_DDCUTIL "DDCUtil library support" OFF)
if(HAVE_DDCUTIL)
    find_package(DDCUtil REQUIRED)
    add_definitions(-DWITH_DDCUTIL)
    set_package_properties(DDCUtil
        PROPERTIES DESCRIPTION "DDCUtil library support"
        TYPE OPTIONAL
        PURPOSE "Set monitor settings over DDC/CI channel"
    )
else()
    add_feature_info("DDCUtil" HAVE_DDCUTIL "DDCUtil library support is disabled by default as recommended by authors, add -DHAVE_DDCUTIL=On to enable")
endif()

include_directories (
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(-DQT_NO_FOREACH)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX POWERDEVIL
                  VERSION_HEADER "${CMAKE_BINARY_DIR}/powerdevil_version.h"
)

add_subdirectory(daemon)
add_subdirectory(kcmodule)
add_subdirectory(doc)

ecm_qt_install_logging_categories(
        EXPORT POWERDEVIL
        FILE powerdevil.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )
install( FILES powerdevil.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR} )

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
