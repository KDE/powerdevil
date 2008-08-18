/***************************************************************************
 *   Copyright (C) 2008 by Lukas Appelhans                                 *
 *   l.appelhans@gmx.de                                                    *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#ifndef POWERDEVILENGINE_H
#define POWERDEVILENGINE_H

#include <plasma/dataengine.h>

#include <QtDBus/QDBusConnection>

static const QString POWERDEVIL_DBUS_SERVICE = "org.kde.kded";
static const QString POWERDEVIL_DBUS_PATH = "/modules/powerdevil";
static const QString POWERDEVIL_DBUS_INTERFACE = "org.kde.powerdevil";
static const uint MINIMUM_UPDATE_INTERVAL = 1000;

class PowerDevilEngine : public Plasma::DataEngine
{
    Q_OBJECT
    Q_PROPERTY(uint refreshTime READ refreshTime WRITE setRefreshTime)
public:
    PowerDevilEngine(QObject* parent, const QVariantList &args);
    ~PowerDevilEngine();

    QStringList sources() const;

    void setRefreshTime(uint time);
    uint refreshTime() const;

protected:
    bool sourceRequestEvent(const QString &name);
    bool updateSourceEvent(const QString& source);

private slots:
    void getPowerDevilData(const QString &name);
    void updatePowerDevilData();
    void connectDBusSlots();

private:
    bool isDBusServiceRegistered();

    QDBusConnection dbus;
    QString currentAction;
    bool dbusError;
    bool slotsAreConnected;
};

K_EXPORT_PLASMA_DATAENGINE(powerdevil, PowerDevilEngine)

#endif /*POWERDEVILENGINE_H*/
