/***************************************************************************
 *   Copyright (C) 2011 by Dario Freddi <drf@kde.org>                      *
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


#include "activitypage.h"

#include "activitywidget.h"

#include <QtGui/QScrollArea>
#include <QtGui/QVBoxLayout>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KAboutData>
#include <KDebug>
#include <KIcon>
#include <KPluginFactory>
#include <KTabWidget>

#include <kworkspace/kactivityconsumer.h>

K_PLUGIN_FACTORY(PowerDevilActivitiesKCMFactory,
                 registerPlugin<ActivityPage>();
                )
K_EXPORT_PLUGIN(PowerDevilActivitiesKCMFactory("powerdevilactivitiesconfig","powerdevil"))

ActivityPage::ActivityPage(QWidget *parent, const QVariantList &args)
    : KCModule(PowerDevilActivitiesKCMFactory::componentData(), parent, args)
    , m_activityConsumer(new KActivityConsumer(this))
{
    setButtons(Apply | Help);

    KAboutData *about =
        new KAboutData("powerdevilactivitiesconfig", "powerdevilactivitiesconfig", ki18n("Activities Power Management Configuration"),
                       "", ki18n("A per-activity configurator of KDE Power Management System"),
                       KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
                       ki18n("From this module, you can fine tune power management settings for each of your activities."));

    about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
                     "http://drfav.wordpress.com");

    setAboutData(about);

    // Build the UI
    KTabWidget *tabWidget = new KTabWidget();
    QVBoxLayout *lay = new QVBoxLayout();

    foreach (const QString &activity, m_activityConsumer->listActivities()) {
        KActivityInfo *info = new KActivityInfo(activity, this);
        QString icon = info->icon();
        QString name = info->name();
        kDebug() << activity << info->isValid() << info->availability();

        QScrollArea *scrollArea = new QScrollArea();
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setFrameShadow(QFrame::Plain);
        scrollArea->setLineWidth(0);
        scrollArea->setWidgetResizable(true);

        ActivityWidget *activityWidget = new ActivityWidget(activity);
        scrollArea->setWidget(activityWidget);

        activityWidget->load();
        m_activityWidgets.append(activityWidget);

        connect(activityWidget, SIGNAL(changed(bool)), this, SIGNAL(changed(bool)));

        tabWidget->addTab(scrollArea, KIcon(icon), name);
    }

    lay->addWidget(tabWidget);
    setLayout(lay);
}

ActivityPage::~ActivityPage()
{

}


void ActivityPage::load()
{
    foreach (ActivityWidget *widget, m_activityWidgets) {
        widget->load();
    }

    emit changed(false);
}

void ActivityPage::save()
{
    foreach (ActivityWidget *widget, m_activityWidgets) {
        widget->save();
    }

    emit changed(false);

    // Ask to refresh status
    QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                       "org.kde.Solid.PowerManagement", "refreshStatus");

    // Perform call
    QDBusConnection::sessionBus().asyncCall(call);
}

void ActivityPage::fillUi()
{

}

void ActivityPage::defaults()
{
    KCModule::defaults();
}

#include "activitypage.moc"
