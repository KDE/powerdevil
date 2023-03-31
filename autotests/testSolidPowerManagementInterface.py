#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=too-many-arguments

import os
import sys
import unittest
from subprocess import PIPE, Popen, check_output
from time import sleep
from typing import Callable

from gi.repository import Gio, GLib
from OrgFreedesktopUPowerMock import OrgFreedesktopUPower, GlibMainLoopThread

class SolidPowerManagementInterfaceTests(unittest.TestCase):
    """
    Tests for org.kde.Solid.PowerManagement interface
    """

    dbus_daemon_pid: str
    upower_interface: OrgFreedesktopUPower
    loop_thread: GlibMainLoopThread
    powerdevil_path: str = ""
    powerdevil: Popen[bytes]

    @classmethod
    def setUpClass(cls) -> None:
        """
        Starts a private DBus session and mocks UPower backend
        """
        lines: list[str] = check_output(['dbus-daemon', '--fork', '--print-address=1', '--print-pid=1', '--session'],
                                        universal_newlines=True).strip().splitlines()
        assert len(lines) == 2, 'expected exactly 2 lines of output from dbus-daemon'
        cls.dbus_daemon_pid = lines[1]
        assert int(cls.dbus_daemon_pid) > 0
        os.environ["DBUS_SYSTEM_BUS_ADDRESS"] = lines[0]
        os.environ["DBUS_SESSION_BUS_ADDRESS"] = lines[0]

        cls.loop_thread = GlibMainLoopThread()
        cls.loop_thread.start()
        cls.upower_interface = OrgFreedesktopUPower()

        # Wait until the interface is online
        count: int = 0
        while count < 10:
            if cls.upower_interface.is_online:
                break
            sleep(0.2)
            count += 1

        debug_env: dict[str, str] = os.environ.copy()
        debug_env["QT_LOGGING_RULES"] = "org.kde.powerdevil.debug=true"
        cls.powerdevil = Popen([cls.powerdevil_path], env=debug_env, stderr=PIPE)

        # Wait until the interface is online
        dbus_object_manager = Gio.DBusObjectManagerClient.new_for_bus_sync(Gio.BusType.SESSION, Gio.DBusObjectManagerClientFlags.DO_NOT_AUTO_START,
                                                                           "org.kde.Solid.PowerManagement", '/', None, None, None)
        count = 0
        while count < 10:
            if dbus_object_manager.get_name_owner() is not None:
                break
            sleep(0.5)
            count += 1

    @classmethod
    def tearDownClass(cls) -> None:
        cls.powerdevil.terminate()
        cls.upower_interface.quit()
        cls.loop_thread.quit()
        Popen(["kill", cls.dbus_daemon_pid])

    def setUp(self) -> None:
        pass

    def tearDown(self) -> None:
        pass

    def wait(self, cond: Callable) -> None:
        """
        Waits until cond() becomes true
        """
        i = 0
        while i < 10:
            if cond():
                return
            sleep(0.5)
            i += 1

        self.fail()

    def call_dbus_with_reply(self, method_name: str) -> GLib.Variant:
        """
        Calls a method in org.kde.Solid.PowerManagement to get a property
        """
        bus: Gio.DBusConnection = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        message: Gio.DBusMessage = Gio.DBusMessage.new_method_call("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                                   "org.kde.Solid.PowerManagement", method_name)
        result_message, _ = bus.send_message_with_reply_sync(message, Gio.DBusSendMessageFlags.NONE, 5000, None)
        return result_message.get_body().get_child_value(0)

    def test_currentProfile(self) -> None:
        """
        <method name="currentProfile">
            <arg type="s" direction="out" />
        </method>
        """
        result: GLib.Variant = self.call_dbus_with_reply("currentProfile")
        self.assertTrue(result.is_of_type(GLib.VariantType('s')), f"{result.get_type_string()}")
        self.assertTrue(result.get_string() == "AC", f"{result.get_string()}")

        self.upower_interface.set_upower_property("OnBattery", GLib.Variant('b', True))
        self.wait(lambda: self.call_dbus_with_reply("currentProfile").get_string() == "Battery")

        self.upower_interface.set_upower_property("OnBattery", GLib.Variant('b', False))
        self.wait(lambda: self.call_dbus_with_reply("currentProfile").get_string() == "AC")

    def test_batteryRemainingTime(self) -> None:
        """
        <method name="batteryRemainingTime">
            <arg type="t" direction="out" />
        </method>
        """
        result: GLib.Variant = self.call_dbus_with_reply("batteryRemainingTime")
        self.assertTrue(result.is_of_type(GLib.VariantType('t')), f"{result.get_type_string()}")
        self.assertTrue(result.get_uint64() == self.upower_interface.device_properties[self.upower_interface.BATTERY_OBJECT_PATH]["TimeToFull"].get_int64() *
                        1000)

        self.upower_interface.set_device_property(self.upower_interface.BATTERY_OBJECT_PATH, "TimeToFull", GLib.Variant('x', 3200))
        self.wait(lambda: self.call_dbus_with_reply("batteryRemainingTime").get_uint64() == 3200 * 1000)


if __name__ == '__main__':
    assert len(sys.argv) >= 2, "The path of powerdevil is not specified"
    SolidPowerManagementInterfaceTests.powerdevil_path = sys.argv.pop()
    unittest.main()
