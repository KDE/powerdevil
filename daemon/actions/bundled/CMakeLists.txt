install(FILES powerdevilbrightnesscontrolaction.desktop
              powerdevildimdisplayaction.desktop
              powerdevildisabledesktopeffectsaction.desktop
              powerdevilrunscriptaction.desktop
              powerdevilsuspendsessionaction.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})

# Brightness control config
set(brightnesscontrolconfig_SRCS brightnesscontrolconfig.cpp)
kde4_add_plugin(powerdevilbrightnesscontrolaction_config
                ${brightnesscontrolconfig_SRCS})
target_link_libraries(powerdevilbrightnesscontrolaction_config
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      powerdevilui)
install(TARGETS powerdevilbrightnesscontrolaction_config DESTINATION ${PLUGIN_INSTALL_DIR})

# Dim display config
set(dimdisplayconfig_SRCS dimdisplayconfig.cpp)
kde4_add_plugin(powerdevildimdisplayaction_config
                ${dimdisplayconfig_SRCS})
target_link_libraries(powerdevildimdisplayaction_config
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      powerdevilui)
install(TARGETS powerdevildimdisplayaction_config DESTINATION ${PLUGIN_INSTALL_DIR})

# Disable effects config
set(disabledesktopeffectsconfig_SRCS disabledesktopeffectsconfig.cpp)
kde4_add_plugin(powerdevildisabledesktopeffectsaction_config
                ${disabledesktopeffectsconfig_SRCS})
target_link_libraries(powerdevildisabledesktopeffectsaction_config
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      powerdevilui)
install(TARGETS powerdevildisabledesktopeffectsaction_config DESTINATION ${PLUGIN_INSTALL_DIR})

# Run script config
set(runscriptconfig_SRCS runscriptconfig.cpp)
kde4_add_plugin(powerdevilrunscriptaction_config
                ${runscriptconfig_SRCS})
target_link_libraries(powerdevilrunscriptaction_config
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      ${KDE4_KIO_LIBS}
                      powerdevilui)
install(TARGETS powerdevilrunscriptaction_config DESTINATION ${PLUGIN_INSTALL_DIR})

# Suspend session config
set(suspendsessionconfig_SRCS suspendsessionconfig.cpp)
kde4_add_plugin(powerdevilsuspendsessionaction_config
                ${suspendsessionconfig_SRCS})
target_link_libraries(powerdevilsuspendsessionaction_config
                      ${KDE4_KDECORE_LIBS}
                      ${KDE4_KDEUI_LIBS}
                      ${KDE4_KIO_LIBS}
                      powerdevilui)
install(TARGETS powerdevilsuspendsessionaction_config DESTINATION ${PLUGIN_INSTALL_DIR})
