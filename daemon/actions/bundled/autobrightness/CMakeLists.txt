# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-2-Clause

set(autobrightness_SRCS
    abstractlightsensor.cpp
    iiosensorproxy.cpp
)

# SensorProxy DBus Interfaces
set_source_files_properties(
   net.hadess.SensorProxy.xml
   org.freedesktop.DBus.Properties.xml
   PROPERTIES
   NO_NAMESPACE true
)
qt_add_dbus_interface(autobrightness_SRCS net.hadess.SensorProxy.xml iiosensorproxyinterface)
qt_add_dbus_interface(autobrightness_SRCS org.freedesktop.DBus.Properties.xml dbuspropertiesinterface)

add_library(autobrightness_static STATIC ${autobrightness_SRCS})
set_property(TARGET autobrightness_static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(autobrightness_static
    Qt::Core
    Qt::DBus
)
