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

#include <config-powerdevil.h>

#include <KPluginFactory>
#include <KAboutData>
#include <klocalizedstring.h>

#include <QtDBus/QDBusMessage>

K_PLUGIN_FACTORY( PowerDevilKCMFactory,
                  registerPlugin<PowerDevilKCM>();
                )
K_EXPORT_PLUGIN( PowerDevilKCMFactory( "kcmpowerdevil" ) )

PowerDevilKCM::PowerDevilKCM( QWidget *parent, const QVariantList & ) :
        KCModule( PowerDevilKCMFactory::componentData(), parent ),
        m_dbus( QDBusConnection::sessionBus() )
{
    QVBoxLayout *lay = new QVBoxLayout( this );
    lay->setMargin( 0 );

    m_widget = new ConfigWidget( this );
    lay->addWidget( m_widget );

    setButtons( Apply | Help );

    connect( m_widget, SIGNAL( changed( bool ) ), SIGNAL( changed( bool ) ) );

    KAboutData *about =
        new KAboutData( "kcmpowerdevil", "powerdevil", ki18n( "PowerDevil Configuration" ),
                        POWERDEVIL_VERSION, ki18n( "A configurator for PowerDevil" ),
                        KAboutData::License_GPL, ki18n( "(c), 2008 Dario Freddi" ),
                        ki18n( "From this module, you can configure the Daemon, create "
                               "and edit powersaving profiles, and see your system's "
                               "capabilities." ) );

    about->addAuthor( ki18n( "Dario Freddi" ), ki18n( "Main Developer" ) , "drf@kdemod.ath.cx",
                      "http://drfav.wordpress.com" );

    setAboutData( about );

    setQuickHelp( i18n( "<h1>PowerDevil configuration</h1> <p>This module lets you configure "
                        "PowerDevil. PowerDevil is a daemon (so it runs in background) that is started "
                        "upon KDE startup.</p> <p>PowerDevil has 2 levels of configuration: a general one, "
                        "that is always applied, and a profile-based one, that lets you configure a specific "
                        "behavior in every situation. You can also have a look at your system capabilities in "
                        "the last tab. To get you started, first configure the options in the first 2 tabs. "
                        "Then switch to the fourth one, and create/edit your profiles. Last but not least, "
                        "assign your profiles in the third Tab. You do not have to restart PowerDevil, just click "
                        "\"Apply\", and you are done.</p>" ) );

    connect( m_widget, SIGNAL( profilesChanged() ), SLOT( streamToDBus() ) );
}

void PowerDevilKCM::load()
{
    m_widget->load();
}

void PowerDevilKCM::save()
{
    m_widget->save();

    streamToDBus();
}

void PowerDevilKCM::defaults()
{

}

void PowerDevilKCM::streamToDBus()
{
    QDBusMessage msg = QDBusMessage::createMethodCall( "org.kde.kded", "/modules/powerdevil",
                                          "org.kde.PowerDevil", "reloadAndStream" );
    m_dbus.call( msg );
    msg = QDBusMessage::createMethodCall( "org.kde.kded", "/modules/powerdevil",
            "org.kde.PowerDevil", "refreshStatus" );
    m_dbus.call( msg );
    msg = QDBusMessage::createMethodCall( "org.kde.kded", "/modules/powerdevil",
            "org.kde.PowerDevil", "setUpPollingSystem" );
    m_dbus.call( msg );
    msg = QDBusMessage::createMethodCall( "org.kde.kded", "/modules/powerdevil",
            "org.kde.PowerDevil", "refreshStatus" );
    m_dbus.call( msg );
}

#include "PowerDevilKCM.moc"
