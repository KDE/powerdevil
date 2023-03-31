# SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux")
    return()
endif()

add_test(
    NAME testSolidPowerManagementInterface
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testSolidPowerManagementInterface.py ${KDE_INSTALL_FULL_LIBEXECDIR}/org_kde_powerdevil
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(testSolidPowerManagementInterface PROPERTIES TIMEOUT 300)
