/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kde.org>                      *
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
#include "ErrorWidget.h"

#include <config-powerdevil.h>

#include <KPluginFactory>
#include <KAboutData>
#include <klocalizedstring.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnectionInterface>

K_PLUGIN_FACTORY(PowerDevilKCMFactory,
                 registerPlugin<PowerDevilKCM>();
                )
K_EXPORT_PLUGIN(PowerDevilKCMFactory("kcmpowerdevil"))

PowerDevilKCM::PowerDevilKCM(QWidget *parent, const QVariantList &) :
        KCModule(PowerDevilKCMFactory::componentData(), parent),
        m_dbus(QDBusConnection::sessionBus())
{
    KGlobal::locale()->insertCatalog("powerdevil");

    m_layout = new QVBoxLayout(this);
    m_layout->setMargin(0);

    setButtons(Apply | Help);

    KAboutData *about =
        new KAboutData("kcmpowerdevil", "powerdevil", ki18n("PowerDevil Configuration"),
                       POWERDEVIL_VERSION, ki18n("A configurator for PowerDevil"),
                       KAboutData::License_GPL, ki18n("Copyright © 2008–2011 Dario Freddi"),
                       ki18n("From this module, you can configure the Daemon, create "
                             "and edit powersaving profiles, and see your system’s "
                             "capabilities."));

    about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
                     "http://drfav.wordpress.com");

    setAboutData(about);

    setQuickHelp(i18n("<h1>PowerDevil configuration</h1> <p>This module lets you configure "
                      "PowerDevil. PowerDevil is a daemon (so it runs in background) that is started "
                      "upon startup.</p> <p>PowerDevil has two levels of configuration: a general one, "
                      "that is always applied, and a profile-based one, that lets you configure a specific "
                      "behavior in every situation. You can also have a look at your system capabilities in "
                      "the last tab. To get you started, first configure the options in the first two tabs. "
                      "Then switch to the fourth one, and create/edit your profiles. Last but not least, "
                      "assign your profiles in the third Tab. You do not have to restart PowerDevil, just click "
                      "\"Apply\", and you are done.</p>"));

    initModule();

}

void PowerDevilKCM::initModule()
{
    QDBusInterface iface("org.kde.kded", "/modules/powerdevil");

    if (iface.isValid()) {
        if (!QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.powerdevil").value()) {
            QDBusConnection conn = QDBusConnection::systemBus();

            if (conn.interface()->isServiceRegistered("org.freedesktop.PowerManagement") ||
                QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.PowerManagement")) {
                initError(i18n("Another power manager has been detected. PowerDevil will not start if "
                               "other power managers are active. If you want to use PowerDevil as your primary "
                               "power manager, please remove the existing one and restart the PowerDevil service."));
                return;
            } else if (conn.interface()->isServiceRegistered("com.novell.powersave")) {
                initError(i18n("It seems powersaved is running on this system. PowerDevil will not start if "
                               "other power managers are active. If you want to use PowerDevil as your primary "
                               "power manager, please stop powersaved and restart the PowerDevil service."));
                return;
            }
        }

        initView();
        return;
    } else {
        initError(i18n("PowerDevil seems not to be started. Either you have its service turned off, "
                       "or there is a problem in D-Bus."));
        return;
    }
}

void PowerDevilKCM::initView()
{
    unloadExistingWidgets();

    m_widget = new ConfigWidget(this);
    m_layout->addWidget(m_widget);

    connect(m_widget, SIGNAL(changed(bool)), SIGNAL(changed(bool)));
    connect(m_widget, SIGNAL(reloadRequest()), SLOT(streamToDBus()));
    connect(m_widget, SIGNAL(reloadModule()), SLOT(forceReload()));
}

void PowerDevilKCM::initError(const QString &error)
{
    unloadExistingWidgets();

    m_error = new ErrorWidget(this);
    m_layout->addWidget(m_error);

    m_error->setError(error);
}

void PowerDevilKCM::unloadExistingWidgets()
{
    if (m_widget) {
        m_widget->deleteLater();
    }

    if (m_error) {
        m_error->deleteLater();
    }
}

void PowerDevilKCM::load()
{
    if (m_widget) {
        m_widget->load();
    }
    emit changed(false);
}

void PowerDevilKCM::save()
{
    if (m_widget) {
        m_widget->save();

        streamToDBus();
    }
    emit changed(false);
}

void PowerDevilKCM::defaults()
{

}

void PowerDevilKCM::forceReload()
{
    unloadExistingWidgets();
    initModule();
    load();
}

void PowerDevilKCM::streamToDBus()
{
    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded", "/modules/powerdevil",
                       "org.kde.PowerDevil", "reloadAndStream");
    m_dbus.asyncCall(msg);
}

#include "PowerDevilKCM.moc"
