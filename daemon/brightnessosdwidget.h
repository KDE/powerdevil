/*******************************************************************
* brightnessosdwidget.h
* adapted from kdemultimedia/kmix/osdwidget.h
* Copyright  2009    Aurélien Gâteau <agateau@kde.org>
* Copyright  2009    Dario Andres Rodriguez <andresbajotierra@gmail.com>
* Copyright  2009    Christian Esken <christian.esken@arcor.de>
* Copyright  2010    Felix Geyer <debfx-kde@fobos.de>
* Copyright  2015    Kai Uwe Broulik <kde@privat.broulik.de>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************/

#ifndef BRIGHTNESSOSDWIDGET_H
#define BRIGHTNESSOSDWIDGET_H

#include "powerdevilbackendinterface.h"

namespace BrightnessOSDWidget
{

    void show(int percentage, PowerDevil::BackendInterface::BrightnessControlType type = PowerDevil::BackendInterface::Screen);

}

#endif // BRIGHTNESSOSDWIDGET_H
