#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=too-many-arguments

from random import randint
import threading
from typing import Any, Final

from gi.repository import Gio, GLib


class SensorProxy:
    """
    A mocked net.hadess.SensorProxy interface implemented in GDBus, since dbus-python does not support property
    """

    BASE_IFACE: Final = "net.hadess.SensorProxy"
    OBJECT_PATH: Final = "/net/hadess/SensorProxy"

    __connection: Gio.DBusConnection

    def __init__(self) -> None:
        self.randomBrightnessTimer = threading.Timer(1, self.random_brightness)

        self.base_properties: dict[str, GLib.Variant] = {
            "HasAccelerometer": GLib.Variant('b', False),
            "AccelerometerOrientation": GLib.Variant('s', ""),
            "HasAmbientLight": GLib.Variant('b', True),
            "LightLevelUnit": GLib.Variant('s', "vendor"),
            "LightLevel": GLib.Variant('d', 0.5),
            "HasProximity": GLib.Variant('b', False),
            "ProximityNear": GLib.Variant('b', False),
        }

        self.__owner_id: int = Gio.bus_own_name(Gio.BusType.SYSTEM, self.BASE_IFACE, Gio.BusNameOwnerFlags.NONE, self.on_bus_acquired,
                                                self.on_name_acquired, self.on_name_lost)
        assert self.__owner_id > 0

    def quit(self) -> None:
        Gio.bus_unown_name(self.__owner_id)
        print("Interface is shut down")

    def random_brightness(self) -> None:
        """
        Sets a random light level
        """
        self.base_properties["LightLevel"] = GLib.Variant('d', randint(0, 100) / 100.0)
        changed_properties = GLib.Variant('a{sv}', {
            "LightLevel": self.base_properties["LightLevel"],
        })
        Gio.DBusConnection.emit_signal(self.__connection, None, self.OBJECT_PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                        GLib.Variant.new_tuple(GLib.Variant('s', self.BASE_IFACE), changed_properties, GLib.Variant('as', ())))
        print(f"New light level {self.base_properties['LightLevel'].get_double()}")

        self.randomBrightnessTimer = threading.Timer(1, self.random_brightness)
        self.randomBrightnessTimer.start()


    def on_bus_acquired(self, connection: Gio.DBusConnection, name: str, *args) -> None:
        """
        Interface is ready, now register objects.
        """
        self.__connection = connection

        properties_introspection_xml: str = '\n'.join(open("../daemon/actions/bundled/autobrightness/net.hadess.SensorProxy.xml", encoding="utf-8").readlines())
        introspection_data = Gio.DBusNodeInfo.new_for_xml(properties_introspection_xml)
        reg_id = connection.register_object(self.OBJECT_PATH, introspection_data.interfaces[0], self.interface_handle_method_call, self.interface_handle_get_property, None)
        assert reg_id != 0

    def on_name_acquired(self, connection, name, *args) -> None:
        pass

    def on_name_lost(self, connection, name, *args) -> None:
        pass

    def interface_handle_method_call(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, method_name: str,
                                     parameters: GLib.Variant, invocation: Gio.DBusMethodInvocation) -> None:
        """
        Handles method calls for net.hadess.SensorProxy
        """
        if method_name == "ClaimLight":
            print("ClaimLight")
            self.randomBrightnessTimer = threading.Timer(1, self.random_brightness)
            self.randomBrightnessTimer.start()
        elif method_name == "ReleaseLight":
            print("ReleaseLight")
            self.randomBrightnessTimer.cancel()
        else:
            print(f"other method name {method_name}")

    def interface_handle_get_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, value: Any):
        """
        Handles properties for net.hadess.SensorProxy
        """
        if value not in self.base_properties.keys():
            print(f"{value} does not exist")
            return None

        print(f"{value} = {self.base_properties[value]}")

        return self.base_properties[value]

    def interface_handle_set_property(self, connection: Gio.DBusConnection, sender: str, object_path: str, interface_name: str, key: str, value: Any) -> bool:
        """
        Handles properties for net.hadess.SensorProxy
        """
        return False


class GlibMainloopThread(threading.Thread):
    """
    Runs Glib main loop in another thread
    """

    def __init__(self) -> None:
        # Set up DBus loop
        self.loop = GLib.MainLoop()
        self.timer = threading.Timer(3000, self.loop.quit)

        # Create the thread
        super(GlibMainloopThread, self).__init__()

    def run(self) -> None:
        """
        Method to run the DBus main loop (on a thread)
        """
        self.timer.start()
        self.loop.run()

    def quit(self) -> None:
        self.timer.cancel()


if __name__ == '__main__':
    loopThread = GlibMainloopThread()
    loopThread.start()
    sensor_proxy_interface = SensorProxy()

