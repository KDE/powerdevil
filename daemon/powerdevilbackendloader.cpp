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


#include <config-powerdevil.h>
#include "powerdevilbackendloader.h"
#include "powerdevil_debug.h"

#ifdef HAVE_UDEV
#include "backends/upower/powerdevilupowerbackend.h"
#endif
#include "backends/hal/powerdevilhalbackend.h"

#include <QDebug>

namespace PowerDevil {
namespace BackendLoader {

BackendInterface* loadBackend(QObject *parent)
{
#ifdef HAVE_UDEV
    // Check UPower first
    qCDebug(POWERDEVIL) << "Loading UPower backend...";
    if (PowerDevilUPowerBackend::isAvailable()) {
        qCDebug(POWERDEVIL) << "Success!";
        return new PowerDevilUPowerBackend(parent);
    }

    qCDebug(POWERDEVIL) << "Failed!";
#endif

    // If we are here, try HAL
    qCDebug(POWERDEVIL) << "Loading HAL backend...";
    if (PowerDevilHALBackend::isAvailable()) {
        qCDebug(POWERDEVIL) << "Success!";
        return new PowerDevilHALBackend(parent);
    }

    // Fail...
    qCDebug(POWERDEVIL) << "Failed!";
    return 0;
}

}
}

