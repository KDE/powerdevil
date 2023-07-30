/*
 *  SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "powerdevilcore_export.h"

namespace PowerDevil
{
Q_NAMESPACE_EXPORT(POWERDEVILCORE_EXPORT)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

// emulate std::to_underlying() from C++23 until we can use the newer standard here
template<typename Enum>
constexpr typename std::enable_if<std::is_enum<Enum>::value, typename std::underlying_type<Enum>::type>::type to_underlying(Enum const &value)
{
    return static_cast<typename std::underlying_type<Enum>::type>(value);
}

// The flag-style values here are useless:
// Power actions never use more than one at a time, and config combobox option listings can't be
// passed as flags because it's impossible to specify whether NoAction is or isn't included.
enum class PowerButtonAction : uint {
    NoAction = 0,
    SuspendToRam = 1,
    SuspendToDisk = 2,
    SuspendHybrid = 4,
    Shutdown = 8,
    PromptLogoutDialog = 16,
    LockScreen = 32,
    TurnOffScreen = 64,
    ToggleScreenOnOff = 128
};
Q_ENUM_NS(PowerButtonAction)

} // namespace PowerDevil
