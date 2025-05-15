cmake_minimum_required(VERSION 3.16)

project(PowerDevil)
set(PROJECT_VERSION "6.3.80")

set(PROJECT_DEP_VERSION "6.2.90")
set(QT_MIN_VERSION "6.7.0")
set(KF6_MIN_VERSION "6.10.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH}  ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMConfiguredInstall)
include(ECMGenerateDBusServiceFile)
include(FeatureSummary)
include(GenerateExportHeader)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMDeprecationSettings)
include(ECMQmlModule)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Widgets DBus)

if (Qt6Gui_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6GuiPrivate ${REQUIRED_QT_VERSION} REQUIRED NO_MODULE)
endif()

find_package(QCoro6 REQUIRED COMPONENTS Core DBus)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Auth
    Config
    Crash
    DBusAddons
    I18n
    IdleTime
    ItemModels
    GlobalAccel
    KIO
    Kirigami
    KCMUtils
    Notifications
    Runner
    Solid
    WindowSystem
    XmlGui
)
find_package(Plasma ${PROJECT_DEP_VERSION} REQUIRED)
find_package(PlasmaActivities ${PROJECT_DEP_VERSION} REQUIRED)
find_package(KF6Screen CONFIG REQUIRED)
find_package(LibKWorkspace ${PROJECT_DEP_VERSION} REQUIRED)

find_package(UDev REQUIRED)

find_package(XCB REQUIRED COMPONENTS XCB RANDR DPMS)

find_package(KF6DocTools ${KF6_MIN_VERSION})
set_package_properties(KF6DocTools PROPERTIES
    DESCRIPTION "Tools to generate documentation"
    TYPE OPTIONAL
)

find_package(DDCUtil)
if(DDCUtil_FOUND)
    add_definitions(-DWITH_DDCUTIL)
    set_package_properties(DDCUtil
        PROPERTIES DESCRIPTION "DDCUtil library support"
        TYPE OPTIONAL
        PURPOSE "Set monitor settings over DDC/CI channel"
    )
endif()

find_package(Qt6WaylandClient COMPONENTS Private)
find_package(PlasmaWaylandProtocols 1.18 REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client)

include_directories (
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon
)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX POWERDEVIL
                  VERSION_HEADER "${CMAKE_BINARY_DIR}/powerdevil_version.h"
)
ecm_set_disabled_deprecation_versions(QT 6.8.0
    KF 6.9.0
)

add_subdirectory(applets)
add_subdirectory(daemon)
add_subdirectory(kcm)
add_subdirectory(kcm_mobile)

if(KF6DocTools_FOUND)
    add_subdirectory(doc)
    kdoctools_install(po)
endif()

add_subdirectory(runners)

add_subdirectory(autotests)

ecm_qt_install_logging_categories(
        EXPORT POWERDEVIL
        FILE powerdevil.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )
install( FILES powerdevil.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR} )

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
