#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=too-many-arguments

import threading
from typing import Any, Final

from gi.repository import Gio, GLib


class OrgFreedesktopUPower:
    """
    D-Bus interfaces for org.freedesktop.UPower and org.freedesktop.UPower.Device
    """

    BUS_NAME: Final = "org.freedesktop.UPower"
    OBJECT_PATH: Final = "/org/freedesktop/UPower"
    DEVICE_IFACE_NAME: Final = "org.freedesktop.UPower.Device"
    BATTERY_OBJECT_PATH: Final = "/org/freedesktop/UPower/devices/battery_BAT0"
    AC_OBJECT_PATH: Final = "/org/freedesktop/UPower/devices/line_power_AC"

    __connection: Gio.DBusConnection
    is_online: bool = False

    def __init__(self) -> None:
        self.__owner_id: int = Gio.bus_own_name(Gio.BusType.SYSTEM, self.BUS_NAME, Gio.BusNameOwnerFlags.NONE, self.on_bus_acquired, None, None)
        assert self.__owner_id > 0

        self.upower_properties: dict[str, GLib.Variant] = {
            "DaemonVersion": GLib.Variant('s', "1.90.0"),
            "OnBattery": GLib.Variant('b', False),
            "LidIsClosed": GLib.Variant('b', False),
            "LidIsPresent": GLib.Variant('b', False),
        }
        self.device_properties: dict[str, dict[str, GLib.Variant]] = {
            self.BATTERY_OBJECT_PATH: {
                "NativePath": GLib.Variant('s', "BAT0"),
                "Vendor": GLib.Variant('s', "KDE-Community"),
                "Model": GLib.Variant('s', "Primary"),
                "Serial": GLib.Variant('s', "00150 2020/04/05"),
                "UpdateTime": GLib.Variant('t', 1680271074),
                "Type": GLib.Variant('u', 2),  # Battery
                "PowerSupply": GLib.Variant('b', True),
                "HasHistory": GLib.Variant('b', False),
                "HasStatistics": GLib.Variant('b', False),
                "Online": GLib.Variant('b', False),
                "Energy": GLib.Variant('d', 20.0),
                "EnergyEmpty": GLib.Variant('d', 0.0),
                "EnergyFull": GLib.Variant('d', 40.0),
                "EnergyFullDesign": GLib.Variant('d', 100.0),
                "EnergyRate": GLib.Variant('d', -20.0),  # Charging
                "Voltage": GLib.Variant('d', 12.184),
                "ChargeCycles": GLib.Variant('i', 88),
                "Luminosity": GLib.Variant('d', 0.0),
                "TimeToEmpty": GLib.Variant('x', 0),
                "TimeToFull": GLib.Variant('x', 3600),
                "Percentage": GLib.Variant('d', 50),
                "Temperature": GLib.Variant('d', 20.0),
                "IsPresent": GLib.Variant('b', True),
                "State": GLib.Variant('u', 1),  # Charging
                "IsRechargeable": GLib.Variant('b', True),
                "Capacity": GLib.Variant('d', 40.0),
                "Technology": GLib.Variant('u', 1),  # Lithium ion
                "BatteryLevel": GLib.Variant('u', 0),  # Unknown
                "WarningLevel": GLib.Variant('u', 1),  # None
                "IconName": GLib.Variant('s', ""),
            },
            self.AC_OBJECT_PATH: {
                "NativePath": GLib.Variant('s', "AC"),
                "Vendor": GLib.Variant('s', ""),
                "Model": GLib.Variant('s', ""),
                "Serial": GLib.Variant('s', ""),
                "UpdateTime": GLib.Variant('t', 1680271074),
                "Type": GLib.Variant('u', 1),  # Line Power
                "PowerSupply": GLib.Variant('b', True),
                "HasHistory": GLib.Variant('b', False),
                "HasStatistics": GLib.Variant('b', False),
                "Online": GLib.Variant('b', True),
                "Energy": GLib.Variant('d', 0.0),
                "EnergyEmpty": GLib.Variant('d', 0.0),
                "EnergyFull": GLib.Variant('d', 0.0),
                "EnergyFullDesign": GLib.Variant('d', 0.0),
                "EnergyRate": GLib.Variant('d', 0.0),
                "Voltage": GLib.Variant('d', 0.0),
                "ChargeCycles": GLib.Variant('i', 0),
                "Luminosity": GLib.Variant('d', 0.0),
                "TimeToEmpty": GLib.Variant('x', 0),
                "TimeToFull": GLib.Variant('x', 0),
                "Percentage": GLib.Variant('d', 0.0),
                "Temperature": GLib.Variant('d', 0.0),
                "IsPresent": GLib.Variant('b', False),
                "State": GLib.Variant('u', 0),  # Unknown
                "IsRechargeable": GLib.Variant('b', False),
                "Capacity": GLib.Variant('d', 0.0),
                "Technology": GLib.Variant('u', 0),  # Unknown
                "WarningLevel": GLib.Variant('u', 0),  # Unknown
                "BatteryLevel": GLib.Variant('u', 0),  # Unknown
                "IconName": GLib.Variant('s', ""),
            },
        }
        self.device_object_paths: GLib.Variant = GLib.Variant("ao", (self.BATTERY_OBJECT_PATH, self.AC_OBJECT_PATH))

    def quit(self) -> None:
        Gio.bus_unown_name(self.__owner_id)

    def set_upower_property(self, property_name: str, value: GLib.Variant) -> None:
        self.upower_properties[property_name] = value
        changed_properties = GLib.Variant('a{sv}', {
            property_name: self.upower_properties[property_name],
        })
        Gio.DBusConnection.emit_signal(self.__connection, None, self.OBJECT_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                       GLib.Variant.new_tuple(GLib.Variant('s', self.BUS_NAME), changed_properties, GLib.Variant('as', ())))

    def set_device_property(self, object_path: str, property_name: str, value: GLib.Variant) -> None:
        self.device_properties[object_path][property_name] = value
        changed_properties = GLib.Variant('a{sv}', {
            property_name: self.device_properties[object_path][property_name],
        })
        Gio.DBusConnection.emit_signal(self.__connection, None, object_path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                       GLib.Variant.new_tuple(GLib.Variant('s', self.DEVICE_IFACE_NAME), changed_properties, GLib.Variant('as', ())))

    def on_bus_acquired(self, connection: Gio.DBusConnection, name: str, *args) -> None:
        """
        Interface is ready, now register objects.
        """
        self.__connection = connection

        with open("../daemon/backends/upower/dbus/org.freedesktop.UPower.xml", encoding="utf-8") as file_handler:
            upower_introspection_xml: str = '\n'.join(file_handler.readlines())
            introspection_data = Gio.DBusNodeInfo.new_for_xml(upower_introspection_xml)
            reg_id = connection.register_object(self.OBJECT_PATH, introspection_data.interfaces[0], self.upower_handle_method_call,
                                                self.upower_handle_get_property, self.upower_handle_set_property)
            assert reg_id > 0

        with open("../daemon/backends/upower/dbus/org.freedesktop.UPower.Device.xml", encoding="utf-8") as file_handler:
            device_introspection_xml: str = ""
            skip_doc = False  # Gio.DBusNodeInfo.new_for_xml doesn't like doc
            for l in file_handler.readlines():
                if not skip_doc:
                    if "<doc:doc>" in l:
                        skip_doc = True
                        continue
                    else:
                        device_introspection_xml += l + '\n'
                else:
                    if "</doc:doc>" in l:
                        skip_doc = False

            introspection_data = Gio.DBusNodeInfo.new_for_xml(device_introspection_xml)
            reg_id = connection.register_object(self.BATTERY_OBJECT_PATH, introspection_data.interfaces[0], self.device_handle_method_call,
                                                self.device_handle_get_property, self.device_handle_set_property)
            assert reg_id > 0

            reg_id = connection.register_object(self.AC_OBJECT_PATH, introspection_data.interfaces[0], self.device_handle_method_call,
                                                self.device_handle_get_property, self.device_handle_set_property)
            assert reg_id > 0

        self.is_online = True

    def upower_handle_method_call(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, method_name: str,
                                  parameters: GLib.Variant, invocation: Gio.DBusMethodInvocation) -> None:
        """
        Handles method calls for org.freedesktop.DBus.Properties and org.freedesktop.UPower
        """
        assert interface_name == self.BUS_NAME, f"Unknown method {method_name}"

        if method_name == "EnumerateDevices":
            invocation.return_value(GLib.Variant.new_tuple(self.device_object_paths))
        elif method_name == "GetDisplayDevice":
            invocation.return_value(GLib.Variant.new_tuple(GLib.Variant('o', '/')))
        elif method_name == "GetCriticalAction":
            invocation.return_value(GLib.Variant.new_tuple(GLib.Variant('s', "PowerOff")))
        else:
            assert False

    def upower_handle_get_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, value: Any):
        """
        Handles properties for org.freedesktop.UPower
        """
        assert value in self.upower_properties.keys(), f"{value} does not exist"

        return self.upower_properties[value]

    def upower_handle_set_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, key: str, value: Any) -> bool:
        """
        Handles properties for org.freedesktop.UPower
        """
        assert False, "Only read-only properties"

    def device_handle_method_call(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, method_name: str,
                                  parameters: GLib.Variant, invocation: Gio.DBusMethodInvocation) -> None:
        """
        Handles method calls for org.freedesktop.UPower.Device
        """
        if interface_name == self.DEVICE_IFACE_NAME:
            assert method_name in ("Refresh", "GetHistory", "GetStatistics"), f"Unknown method {method_name}"
        else:
            assert False, f"Unknown interface {interface_name}"

    def device_handle_get_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, value: Any):
        """
        Handles properties for org.freedesktop.UPower.Device
        """
        assert interface_name == self.DEVICE_IFACE_NAME, f"Wrong interface name {interface_name} from {sender}"
        assert object_path in self.device_properties.keys(), f"Unknown object path {object_path}"
        assert value in self.device_properties[object_path].keys(), f"Unknown property {value} in {object_path}"

        return self.device_properties[object_path][value]

    def device_handle_set_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, key: str, value: Any) -> bool:
        """
        Handles properties for org.freedesktop.UPower.Device
        """
        assert False, "Only read-only properties"


class GlibMainLoopThread(threading.Thread):
    """
    Runs Glib main loop in another thread
    """

    def __init__(self) -> None:
        # Set up D-Bus loop
        self.loop = GLib.MainLoop()
        self.timer = threading.Timer(30, self.loop.quit)

        # Create the thread
        super(GlibMainLoopThread, self).__init__()

    def run(self) -> None:
        """
        Method to run the DBus main loop (on a thread)
        """
        self.timer.start()
        self.loop.run()

    def quit(self) -> None:
        self.timer.cancel()
        self.loop.quit()
