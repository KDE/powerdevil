add_definitions(-DTRANSLATION_DOMAIN=\"powerdevil\" -DQT_USE_QSTRINGBUILDER)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(actions)

set(POWERDEVIL_CORE_VERSION_MAJOR 2)
set(POWERDEVIL_CORE_VERSION_MINOR 0)
set(POWERDEVIL_CORE_VERSION_MICRO 0)
set(POWERDEVIL_CORE_VERSION_STRING ${POWERDEVIL_CORE_VERSION_MAJOR}.${POWERDEVIL_CORE_VERSION_MINOR}.${POWERDEVIL_CORE_VERSION_MICRO})

# Add bundled actions
set(powerdevil_bundled_actions_SRCS
    powerdevil_debug.cpp
    actions/bundled/suspendsession.cpp
    actions/bundled/brightnesscontrol.cpp
    actions/bundled/keyboardbrightnesscontrol.cpp
    actions/bundled/dimdisplay.cpp
    actions/bundled/runscript.cpp
    actions/bundled/handlebuttonevents.cpp
)

# target no.1 - powerdevil core library
set(powerdevilcore_SRCS
    powerdevil_debug.cpp
    powerdevilaction.cpp
    powerdevilactionpool.cpp
    powerdevilbackendinterface.cpp
    powerdevilcore.cpp
    powerdevilpolicyagent.cpp
    powerdevilprofilegenerator.cpp
    powerdevilbrightnesslogic.cpp
    powerdevilscreenbrightnesslogic.cpp
    powerdevilkeyboardbrightnesslogic.cpp
    brightnessosdwidget.cpp
    kwinkscreenhelpereffect.cpp
)

kconfig_add_kcfg_files(powerdevilcore_SRCS ../PowerDevilSettings.kcfgc)

# Action DBus Adaptors
qt5_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.BrightnessControl.xml
                     actions/bundled/brightnesscontrol.h PowerDevil::BundledActions::BrightnessControl)
qt5_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl.xml
                     actions/bundled/keyboardbrightnesscontrol.h PowerDevil::BundledActions::KeyboardBrightnessControl)
qt5_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.HandleButtonEvents.xml
                     actions/bundled/handlebuttonevents.h PowerDevil::BundledActions::HandleButtonEvents)
qt5_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.SuspendSession.xml
                     actions/bundled/suspendsession.h PowerDevil::BundledActions::SuspendSession)

add_library(powerdevilcore SHARED ${powerdevilcore_SRCS} ${powerdevil_bundled_actions_SRCS})
set_target_properties(powerdevilcore PROPERTIES VERSION ${POWERDEVIL_CORE_VERSION_STRING} SOVERSION ${POWERDEVIL_CORE_VERSION_MAJOR})

# not exported, so just make the deps public
target_link_libraries(powerdevilcore
    KF5::Activities
    KF5::ConfigCore
    KF5::ConfigGui
    KF5::DBusAddons
    KF5::I18n
    KF5::Solid
    KF5::IdleTime
    KF5::GlobalAccel
    KF5::Screen
    KF5::Service
    KF5::WidgetsAddons
    KF5::Notifications
    KF5::XmlGui
    PW::KWorkspace
)

if (XCB_FOUND) # kwin kscreen helper effect
    target_link_libraries(powerdevilcore Qt5::X11Extras XCB::XCB)
endif ()

# target no.2 - powerdevil kded module
set(kded_powerdevil_SRCS
    powerdevil_debug.cpp
    kdedpowerdevil.cpp
    powerdevilbackendloader.cpp
    powerdevilfdoconnector.cpp
)

# DBus Adaptors
qt5_add_dbus_adaptor(kded_powerdevil_SRCS org.kde.Solid.PowerManagement.xml powerdevilcore.h PowerDevil::Core)
qt5_add_dbus_adaptor(kded_powerdevil_SRCS org.kde.Solid.PowerManagement.PolicyAgent.xml
                                         powerdevilpolicyagent.h PowerDevil::PolicyAgent
                                         powermanagementpolicyagentadaptor PowerManagementPolicyAgentAdaptor)

qt5_add_dbus_adaptor(kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.PowerManagement.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementfdoadaptor PowerManagementFdoAdaptor)
qt5_add_dbus_adaptor(kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.PowerManagement.Inhibit.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor)


# Backends
include(BackendConfig.cmake)

add_library(kded_powerdevil MODULE ${kded_powerdevil_SRCS} ${POWERDEVIL_BACKEND_SRCS})
set_target_properties(kded_powerdevil PROPERTIES OUTPUT_NAME powerdevil)
kcoreaddons_desktop_to_json(kded_powerdevil powerdevil.desktop)

target_link_libraries(kded_powerdevil
    ${POWERDEVIL_BACKEND_LIBS}
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::DBusAddons
    powerdevilcore
)
if (XCB_FOUND)
    target_link_libraries(kded_powerdevil Qt5::X11Extras)
endif ()

target_link_libraries(kded_powerdevil ${UDEV_LIBS})

install(TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR}/kf5/kded)
install(TARGETS powerdevilcore ${INSTALL_TARGETS_DEFAULT_ARGS})

# target no.3 - powerdevil ui library
set(powerdevilui_SRCS
    powerdevilactionconfig.cpp
)

add_library(powerdevilui ${powerdevilui_SRCS})
set_target_properties(powerdevilui PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# not exported, so just make the deps public
target_link_libraries(powerdevilui
    PUBLIC
        Qt5::Widgets
        KF5::ConfigCore
)

set(HAVE_XCB ${XCB_FOUND})
configure_file(config-powerdevil.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-powerdevil.h )

install(TARGETS powerdevilui ${INSTALL_TARGETS_DEFAULT_ARGS})
