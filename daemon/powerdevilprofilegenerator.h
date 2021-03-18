/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#pragma once

namespace PowerDevil {

namespace ProfileGenerator
{
    enum Modes {
        NoneMode = 0,
        ToRamMode = 1,
        ToDiskMode = 2,
        SuspendHybridMode = 4,
        ShutdownMode = 8,
        LogoutDialogMode = 16,
        LockScreenMode = 32,
        TurnOffScreenMode = 64,
        ToggleScreenOnOffMode = 128,
    };

    void generateProfiles(bool isMobile, bool toRam, bool toDisk);
}

}
