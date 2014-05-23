/*******************************************************************
* brightnessosdwidget.cpp
* adapted from kdemultimedia/kmix/osdwidget.cpp
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

#include "brightnessosdwidget.h"

// Qt
#include <QObject>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCall>

BrightnessOSDWidget::BrightnessOSDWidget(PowerDevil::BackendInterface::BrightnessControlType type, QObject *parent)
    : QObject(parent)
    , m_type(type)
{

}

BrightnessOSDWidget::~BrightnessOSDWidget()
{
}

void BrightnessOSDWidget::setCurrentBrightness(int brightnessLevel)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1Literal("org.kde.plasmashell"),
        QLatin1Literal("/org/kde/osdService"),
        QLatin1Literal("org.kde.osdService"),
        QLatin1Literal("brightnessChanged")
    );

    msg.setArguments(QList<QVariant>() << brightnessLevel);

    QDBusConnection::sessionBus().asyncCall(msg);
}
#include "brightnessosdwidget.moc"
