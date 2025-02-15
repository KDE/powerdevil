# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

#! /usr/bin/env bash
$XGETTEXT `find . -name "*.cpp" -o -name "*.qml"` -o $podir/kcm_energyinfo.pot
