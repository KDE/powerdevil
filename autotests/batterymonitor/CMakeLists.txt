# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

add_test(
    NAME batterymonitortest_withdisplaydevice
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh --failfast
)
set_tests_properties(batterymonitortest_withdisplaydevice PROPERTIES TIMEOUT 120 ENVIRONMENT "ENABLE_DISPLAY_DEVICE=0;POWERDEVIL_PATH=${KDE_INSTALL_FULL_LIBEXECDIR}/org_kde_powerdevil")

add_test(
    NAME batterymonitortest_withoutdisplaydevice
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh --failfast
)
set_tests_properties(batterymonitortest_withoutdisplaydevice PROPERTIES TIMEOUT 120 ENVIRONMENT "ENABLE_DISPLAY_DEVICE=0;POWERDEVIL_PATH=${KDE_INSTALL_FULL_LIBEXECDIR}/org_kde_powerdevil")
