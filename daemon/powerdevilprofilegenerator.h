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

#ifndef POWERDEVIL_PROFILEGENERATOR_H
#define POWERDEVIL_PROFILEGENERATOR_H

#include <QString>

namespace PowerDevil {

namespace ProfileGenerator
{
    // the new style of settings. use this profileSettingsName() + "_AC" to open the AC profile.
    Q_DECL_EXPORT QString profileSettingsBaseName();

    // Original settings name - with multiple profiles per file.
    QString oldProfileSettingsName();

    void generateProfiles(bool isMobile, bool toRam, bool toDisk);

    // True if ported, false if there's nothing to port.
    bool portDeprecatedConfig(const QString& profileName, bool toRam, bool toDisk);

    bool hasOldConfig(const QString& profileName);
}

}

#endif // POWERDEVIL_PROFILEGENERATOR_H
