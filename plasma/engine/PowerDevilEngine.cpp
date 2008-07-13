/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@gmail.com                                                    *
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

#include "PowerDevilEngine.h"

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

#include "plasma/datacontainer.h"

PowerDevilEngine::PowerDevilEngine(QObject *parent, const QVariantList &args)
  : Plasma::DataEngine(parent),
  dbus(QDBusConnection::sessionBus()),
  dbusError(false),
  slotsAreConnected(false)
{
	Q_UNUSED(args)
	
	setMinimumPollingInterval(MINIMUM_UPDATE_INTERVAL);
	
}

PowerDevilEngine::~PowerDevilEngine()
{
}

QStringList PowerDevilEngine::sources() const
{
    QStringList sources;
    sources << "PowerDevil";

    return sources;
}

void PowerDevilEngine::setRefreshTime(uint time)
{
        setPollingInterval(time);
}

uint PowerDevilEngine::refreshTime() const
{
        return 1000;
}

bool PowerDevilEngine::sourceRequestEvent(const QString &name)
{
	return updateSourceEvent(name);
}

bool PowerDevilEngine::updateSourceEvent(const QString &name)
{
	if(!name.compare("PowerDevil", Qt::CaseInsensitive))
		getPowerDevilData(name);
	
	return true;
}

void PowerDevilEngine::getPowerDevilData(const QString &name)
{
	removeAllData(name);

	if(isDBusServiceRegistered()) 
	{
		setData(name, "error", false);
	}
	else 
	{
		setData(name, "error", true);
		setData(name, "errorMessage", I18N_NOOP("Is PowerDevil up and running?"));
	}
}

bool PowerDevilEngine::isDBusServiceRegistered()
{
	QStringList modules;
	QDBusInterface kdedInterface( "org.kde.kded", "/kded", "org.kde.kded" );
	QDBusReply<QStringList> reply = kdedInterface.call( "loadedModules"  );

	if ( reply.isValid() ) {
		modules = reply.value();
	}

	if ( modules.contains("powerdevil") )
	{
		if(!slotsAreConnected)
		{
			connectDBusSlots();
			slotsAreConnected = true;
		}
		return true;
	}
	else
	{
		if(slotsAreConnected)
		{
			slotsAreConnected = false;
		}
		return false;
	}
}

void PowerDevilEngine::updatePowerDevilData()
{
	getPowerDevilData("PowerDevil");
}

void PowerDevilEngine::connectDBusSlots()
{
	/*if(!dbus.connect("org.archlinux.shaman", "/PowerDevil", "org.archlinux.shaman", 
			"actionStatusChanged", this, SLOT(actionChanged(const QString&))))
		dbusError = true;
	else
		dbusError = false;*/
}

#include "PowerDevilEngine.moc"
