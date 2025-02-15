# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2025 Ismael Asensio <isma.af@gmail.com>

add_definitions(-DTRANSLATION_DOMAIN=\"kcm_energyinfo\")

kcmutils_add_qml_kcm(kcm_energyinfo INSTALL_NAMESPACE "plasma/kcms")

target_sources(kcm_energyinfo PRIVATE
    kcm.cpp
    batterymodel.cpp batterymodel.h
    statisticsprovider.cpp statisticsprovider.h
)

target_link_libraries(kcm_energyinfo PRIVATE
  Qt::DBus
  Qt::Gui
  KF6::CoreAddons
  KF6::I18n
  KF6::KCMUtilsQuick
  KF6::Solid
)
