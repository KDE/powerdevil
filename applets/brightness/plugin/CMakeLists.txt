# SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
# SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

ecm_add_qml_module(brightnesscontrolplugin URI org.kde.plasma.private.brightnesscontrolplugin GENERATE_PLUGIN_SOURCE)
target_sources(brightnesscontrolplugin PRIVATE
    nightlightinhibitor.cpp nightlightinhibitor.h
    nightlightcontrol.cpp nightlightcontrol.h
    screenbrightnesscontrol.cpp screenbrightnesscontrol.h
    screenbrightnessdisplaymodel.cpp screenbrightnessdisplaymodel.h
    keyboardbrightnesscontrol.cpp
    keyboardcolorcontrol.cpp keyboardcolorcontrol.h
)
target_link_libraries(brightnesscontrolplugin PRIVATE
    Qt::Core
    Qt::DBus
    Qt::Qml
    QCoro::Coro
    QCoro::DBus
)

ecm_qt_declare_logging_category(brightnesscontrolplugin
    HEADER "brightnesscontrolplugin_debug.h"
    IDENTIFIER "APPLETS::BRIGHTNESS"
    CATEGORY_NAME org.kde.applets.brightness
    DEFAULT_SEVERITY Warning
    DESCRIPTION "Brightness and Color applet" EXPORT "APPLETS::BRIGHTNESS"
)

ecm_finalize_qml_module(brightnesscontrolplugin)
