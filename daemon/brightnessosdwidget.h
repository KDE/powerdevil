/*******************************************************************
* brightnessosdwidget.h
* adapted from kdemultimedia/kmix/osdwidget.h
* Copyright  2009    Aurélien Gâteau <agateau@kde.org>
* Copyright  2009    Dario Andres Rodriguez <andresbajotierra@gmail.com>
* Copyright  2009    Christian Esken <christian.esken@arcor.de>
* Copyright  2010    Felix Geyer <debfx-kde@fobos.de>
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

#ifndef BRIGHTNESSOSDWIDGET__H
#define BRIGHTNESSOSDWIDGET__H

#include "powerdevilbackendinterface.h"

#include <QDBusInterface>

class BrightnessOSDWidget : public QObject
{
Q_OBJECT
public:
    BrightnessOSDWidget(PowerDevil::BackendInterface::BrightnessControlType type, QObject* parent = 0);
    virtual ~BrightnessOSDWidget();

    void setCurrentBrightness(int brightnessLevel);

private:
    void initDBus();
    int m_brightness;
    PowerDevil::BackendInterface::BrightnessControlType m_type;
};

#endif
