/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#include "PowerDevilKCM.h"

#include "ConfigWidget.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KAboutData>
#include <klocalizedstring.h>

#include <QLayout>
#include <QtDBus/QDBusMessage>

K_PLUGIN_FACTORY(PowerDevilKCMFactory,
        registerPlugin<PowerDevilKCM>();
        )
K_EXPORT_PLUGIN(PowerDevilKCMFactory("kcmpowerdevil"))

PowerDevilKCM::PowerDevilKCM(QWidget *parent, const QVariantList &):
	KCModule(PowerDevilKCMFactory::componentData(), parent),
	m_dbus(QDBusConnection::sessionBus())
{
	QVBoxLayout *lay = new QVBoxLayout(this);
	lay->setMargin(0);

	m_widget = new ConfigWidget(this);
	lay->addWidget(m_widget);
	
	setButtons( Apply );
	
	connect(m_widget, SIGNAL(changed(bool)), SIGNAL(changed(bool)));

	//TODO: Add yourself to copyright here
	KAboutData *about =
	new KAboutData(I18N_NOOP("kcmpowerdevil"), 0, ki18n("PowerDevil Configuration"),
			0, KLocalizedString(), KAboutData::License_GPL,
			ki18n("(c), 2008 Dario Freddi"));

	about->addAuthor(ki18n("Dario Freddi"), KLocalizedString() , "drf@kdemod.ath.cx");
	setAboutData( about );
}

void PowerDevilKCM::load()
{
	m_widget->load();
}

void PowerDevilKCM::save()
{
	m_widget->save();
	
	QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded", "/modules/kded_powerdevil", "org.kde.PowerDevil", "refreshStatus");
        m_dbus.call(msg);
}

void PowerDevilKCM::defaults()
{
  
}

#include "PowerDevilKCM.moc"
