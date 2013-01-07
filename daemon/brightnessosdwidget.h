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

#include <QPixmap>

#include <Plasma/Dialog>

class QTimer;

namespace Plasma
{
class Label;
class Meter;
}

class BrightnessOSDWidget : public Plasma::Dialog
{
Q_OBJECT
public:
    BrightnessOSDWidget(QWidget * parent = 0);

    void setCurrentBrightness(int brightnessLevel);
    void activateOSD();

protected slots:
    void themeUpdated();

private:
    QGraphicsScene *m_scene;
    QGraphicsWidget *m_container;
    Plasma::Label *m_iconLabel;
    Plasma::Label *m_volumeLabel;
    Plasma::Meter *m_meter;
    QTimer *m_hideTimer;

    QPixmap m_brightnessPixmap;
};

#endif
