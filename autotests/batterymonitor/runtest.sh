#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

if [ ! -f "./tmp/appiumtests/batterymonitortest.py" ]; then
mkdir -p ./tmp/applets/batterymonitor/dbus || exit 1
wget https://invent.kde.org/plasma/plasma-workspace/-/raw/master/applets/batterymonitor/dbus/org.freedesktop.UPower.xml -O ./tmp/applets/batterymonitor/dbus/org.freedesktop.UPower.xml || exit 1
wget https://invent.kde.org/plasma/plasma-workspace/-/raw/master/applets/batterymonitor/dbus/org.freedesktop.UPower.Device.xml -O ./tmp/applets/batterymonitor/dbus/org.freedesktop.UPower.Device.xml || exit 1

mkdir -p ./tmp/appiumtests/utils || exit 1
wget https://invent.kde.org/plasma/plasma-workspace/-/raw/master/appiumtests/utils/GLibMainLoopThread.py -O ./tmp/appiumtests/utils/GLibMainLoopThread.py || exit 1
wget https://invent.kde.org/plasma/plasma-workspace/-/raw/master/appiumtests/utils/OrgFreedesktopUPower.py -O ./tmp/appiumtests/utils/OrgFreedesktopUPower.py || exit 1
wget https://invent.kde.org/plasma/plasma-workspace/-/raw/master/appiumtests/batterymonitortest.py -O ./tmp/appiumtests/batterymonitortest.py || exit 1
chmod +x ./tmp/appiumtests/batterymonitortest.py || exit 1
fi

pushd ./tmp/appiumtests

selenium-webdriver-at-spi-run ./batterymonitortest.py $@
RET=$?

popd

exit $RET
