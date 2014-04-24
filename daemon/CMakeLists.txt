include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(actions)

set(POWERDEVIL_CORE_VERSION_MAJOR 1)
set(POWERDEVIL_CORE_VERSION_MINOR 1)
set(POWERDEVIL_CORE_VERSION_MICRO 0)
set(POWERDEVIL_CORE_VERSION_STRING ${POWERDEVIL_CORE_VERSION_MAJOR}.${POWERDEVIL_CORE_VERSION_MINOR}.${POWERDEVIL_CORE_VERSION_MICRO})

# Add bundled actions
set(powerdevil_bundled_actions_SRCS
    actions/bundled/suspendsession.cpp
    actions/bundled/brightnesscontrol.cpp
    actions/bundled/keyboardbrightnesscontrol.cpp
    actions/bundled/dimdisplay.cpp
    actions/bundled/runscript.cpp
    actions/bundled/handlebuttonevents.cpp
)

# target no.1 - powerdevil core library
set(powerdevilcore_SRCS
    powerdevilaction.cpp
    powerdevilactionpool.cpp
    powerdevilbackendinterface.cpp
    powerdevilcore.cpp
    powerdevilpolicyagent.cpp
    powerdevilprofilegenerator.cpp
    brightnessosdwidget.cpp
)

kconfig_add_kcfg_files(powerdevilcore_SRCS ../PowerDevilSettings.kcfgc)

qt5_add_dbus_interface(powerdevilcore_SRCS ${SCREENSAVER_DBUS_INTERFACE} screensaver_interface )

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
    PUBLIC
        KF5::KDE4Support
        KF5::Solid
        KF5::IdleTime
        KF5::GlobalAccel
        PW::KWorkspace
)

# target no.2 - powerdevil kded module
set(kded_powerdevil_SRCS
    kdedpowerdevil.cpp
    powerdevilbackendloader.cpp
    powerdevilfdoconnector.cpp
)

# DBus Adaptors
qt5_add_dbus_adaptor(kded_powerdevil_SRCS org.kde.Solid.PowerManagement.xml powerdevilcore.h PowerDevil::Core)
qt5_add_dbus_adaptor(kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/kf5_org.kde.Solid.PowerManagement.PolicyAgent.xml
                                         powerdevilpolicyagent.h PowerDevil::PolicyAgent
                                         powermanagementpolicyagentadaptor PowerManagementPolicyAgentAdaptor)

qt5_add_dbus_adaptor(kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.PowerManagement.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementfdoadaptor PowerManagementFdoAdaptor)
qt5_add_dbus_adaptor(kded_powerdevil_SRCS ${KDE4_DBUS_INTERFACES_DIR}/kf5_org.freedesktop.PowerManagement.Inhibit.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor)


# Backends
include(BackendConfig.cmake)

add_library(kded_powerdevil MODULE ${kded_powerdevil_SRCS} ${POWERDEVIL_BACKEND_SRCS})

target_link_libraries(kded_powerdevil
    ${POWERDEVIL_BACKEND_LIBS}
    KF5::KDE4Support
    KF5::DBusAddons
    powerdevilcore
)
if (X11_FOUND)
    target_link_libraries(kded_powerdevil Qt5::X11Extras)
endif ()

if (UDEV_FOUND)
    target_link_libraries(kded_powerdevil ${UDEV_LIBS})
endif ()

install(TARGETS kded_powerdevil DESTINATION ${PLUGIN_INSTALL_DIR})
install(TARGETS powerdevilcore ${INSTALL_TARGETS_DEFAULT_ARGS})

# target no.3 - powerdevil ui library
set(powerdevilui_SRCS
    powerdevilactionconfig.cpp
)

add_library(powerdevilui ${powerdevilui_SRCS})
set_target_properties(powerdevilui PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )

# not exported, so just make the deps public
target_link_libraries(powerdevilui
    PUBLIC
        Qt5::Widgets
        KF5::ConfigCore
)

set(HAVE_UDEV ${UDEV_FOUND})
configure_file(config-powerdevil.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-powerdevil.h )

install(TARGETS powerdevilui ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES powerdevil.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kded)
