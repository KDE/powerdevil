#pragma once

/*
 * Copyright 2021 Tomaz Canabrava <tcanabrava@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QObject>

class PowerDevilEnums : public QObject {
    Q_OBJECT

public:
    PowerDevilEnums();

    enum PowerButtonMode {
         NoneMode = 0,
         ToRamMode = 1,
         ToDiskMode = 2,
         SuspendHybridMode = 4,
         ShutdownMode = 8,
         LogoutDialogMode = 16,
         LockScreenMode = 32,
         TurnOffScreenMode = 64,
         ToggleScreenOnOffMode = 128
    };
    Q_ENUM(PowerButtonMode)

    enum WirelessMode {
        NoAction,
        TurnOff,
        TurnOn
    };
    Q_ENUM(WirelessMode)

    enum RunScriptMode {
        OnProfileLoad,
        OnProfileUnload,
        After
    };
    Q_ENUM(RunScriptMode)
};
