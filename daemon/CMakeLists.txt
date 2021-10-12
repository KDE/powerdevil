add_definitions(-DTRANSLATION_DOMAIN=\"powerdevil\" -DQT_USE_QSTRINGBUILDER)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(actions)

# Add bundled actions
set(powerdevil_bundled_actions_SRCS
    powerdevil_debug.cpp
    actions/bundled/suspendsession.cpp
    actions/bundled/brightnesscontrol.cpp
    actions/bundled/keyboardbrightnesscontrol.cpp
    actions/bundled/dimdisplay.cpp
    actions/bundled/runscript.cpp
    actions/bundled/handlebuttonevents.cpp
    actions/bundled/dpms.cpp
    actions/bundled/abstractdpmshelper.cpp
    actions/bundled/xcbdpmshelper.cpp
    actions/bundled/waylanddpmshelper.cpp
    ${PowerDevil_SOURCE_DIR}/daemon/kwinkscreenhelpereffect.cpp
    actions/bundled/powerprofile.cpp
)


if(HAVE_WIRELESS_SUPPORT)
    set(powerdevil_bundled_actions_SRCS ${powerdevil_bundled_actions_SRCS}
        actions/bundled/wirelesspowersaving.cpp
    )
endif()

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
    powerdevilpowermanagement.cpp
    brightnessosdwidget.cpp
    kwinkscreenhelpereffect.cpp
)

ecm_qt_declare_logging_category(powerdevilcore_SRCS
   HEADER powerdevil_debug.h
   IDENTIFIER POWERDEVIL
   CATEGORY_NAME org.kde.powerdevil
   DEFAULT_SEVERITY Warning
   DESCRIPTION "Powerdevil" EXPORT POWERDEVIL)

kconfig_add_kcfg_files(powerdevilcore_SRCS ../PowerDevilSettings.kcfgc)

# Action DBus Adaptors
qt_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.BrightnessControl.xml
                     actions/bundled/brightnesscontrol.h PowerDevil::BundledActions::BrightnessControl)
qt_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl.xml
                     actions/bundled/keyboardbrightnesscontrol.h PowerDevil::BundledActions::KeyboardBrightnessControl)
qt_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.HandleButtonEvents.xml
                     actions/bundled/handlebuttonevents.h PowerDevil::BundledActions::HandleButtonEvents)
qt_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.SuspendSession.xml
                     actions/bundled/suspendsession.h PowerDevil::BundledActions::SuspendSession)
if(HAVE_WIRELESS_SUPPORT)
    qt_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.WirelessPowerSaving.xml
                         actions/bundled/wirelesspowersaving.h PowerDevil::BundledActions::WirelessPowerSaving)
endif()
qt5_add_dbus_adaptor(powerdevilcore_SRCS actions/bundled/org.kde.Solid.PowerManagement.Actions.PowerProfile.xml
                         actions/bundled/powerprofile.h PowerDevil::BundledActions::PowerProfile)

qt_add_dbus_interface(powerdevilcore_SRCS org.freedesktop.ScreenSaver.xml screenlocker_interface)

qt5_add_dbus_interface(powerdevilcore_SRCS actions/bundled/org.freedesktop.DBus.Properties.xml properties_interface)

qt5_add_dbus_interface(powerdevilcore_SRCS actions/bundled/net.hadess.PowerProfiles.xml power_profiles_interface)

add_library(powerdevilcore SHARED ${powerdevilcore_SRCS} ${powerdevil_bundled_actions_SRCS})
set_target_properties(powerdevilcore PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 2)

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
    KF5::WaylandClient
    PW::KWorkspace
    XCB::DPMS
    powerdevilsettings
)

if(HAVE_WIRELESS_SUPPORT)
    target_link_libraries(powerdevilcore
        KF5::NetworkManagerQt
        KF5::BluezQt
    )
endif()

if (XCB_FOUND) # kwin kscreen helper effect
    target_link_libraries(powerdevilcore Qt::X11Extras XCB::XCB)
endif ()

# target no.2 - powerdevil kded module
set(powerdevil_SRCS
    powerdevil_debug.cpp
    powerdevilapp.cpp
    powerdevilfdoconnector.cpp
)

# DBus Adaptors
qt_add_dbus_adaptor(powerdevil_SRCS org.kde.Solid.PowerManagement.xml powerdevilcore.h PowerDevil::Core)
qt_add_dbus_adaptor(powerdevil_SRCS org.kde.Solid.PowerManagement.PolicyAgent.xml
                                         powerdevilpolicyagent.h PowerDevil::PolicyAgent
                                         powermanagementpolicyagentadaptor PowerManagementPolicyAgentAdaptor)

qt_add_dbus_adaptor(powerdevil_SRCS org.freedesktop.PowerManagement.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementfdoadaptor PowerManagementFdoAdaptor)
qt_add_dbus_adaptor(powerdevil_SRCS org.freedesktop.PowerManagement.Inhibit.xml powerdevilfdoconnector.h PowerDevil::FdoConnector powermanagementinhibitadaptor PowerManagementInhibitAdaptor)

# KAuth charge threshold helper
add_executable(chargethresholdhelper chargethresholdhelper.cpp powerdevil_debug.cpp ${chargethresholdhelper_mocs})
target_link_libraries(chargethresholdhelper Qt::Core KF5::AuthCore)
install(TARGETS chargethresholdhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(chargethresholdhelper org.kde.powerdevil.chargethresholdhelper root)
kauth_install_actions(org.kde.powerdevil.chargethresholdhelper chargethreshold_helper_actions.actions)

# Backends
add_subdirectory(backends)

add_executable(powerdevil ${powerdevil_SRCS})
set_target_properties(powerdevil PROPERTIES OUTPUT_NAME org_kde_powerdevil)
configure_file(powerdevil.desktop.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/powerdevil.desktop)

target_link_libraries(powerdevil
    KF5::Crash
    KF5::ConfigCore
    KF5::CoreAddons
    KF5::I18n
    KF5::DBusAddons
    PW::KWorkspace
    powerdevilcore
)
if (XCB_FOUND)
    target_link_libraries(powerdevil Qt::X11Extras)
endif ()

target_link_libraries(powerdevil ${UDEV_LIBS})

install(TARGETS powerdevil DESTINATION ${KDE_INSTALL_FULL_LIBEXECDIR})
if (HAVE_LIBCAP)
    install(
    CODE "execute_process(
            COMMAND
                ${SETCAP_EXECUTABLE}
                CAP_WAKE_ALARM=+ep
                \$ENV{DESTDIR}${KDE_INSTALL_FULL_LIBEXECDIR}/org_kde_powerdevil)"
    )
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/powerdevil.desktop
        DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
install(TARGETS powerdevilcore ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# target no.3 - powerdevil ui library
set(powerdevilui_SRCS
    powerdevilactionconfig.cpp
)

add_library(powerdevilui ${powerdevilui_SRCS})
set_target_properties(powerdevilui PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# not exported, so just make the deps public
target_link_libraries(powerdevilui
    PUBLIC
        Qt::Widgets
        KF5::ConfigCore
        powerdevilsettings
)

set(HAVE_XCB ${XCB_FOUND})
configure_file(config-powerdevil.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-powerdevil.h )

install(TARGETS powerdevilui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
ecm_install_configured_files(INPUT plasma-powerdevil.service.in @ONLY DESTINATION
        ${SYSTEMD_USER_UNIT_INSTALL_DIR})
