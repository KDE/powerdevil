/***************************************************************************
 *   Copyright (C) 2011 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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

#include <powerdevil_debug.h>

#include <ErrorOverlay.h>

#include <QScrollArea>
#include <QVBoxLayout>

#include <QtWidgets/QTabWidget>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <KAboutData>
#include <QDebug>
#include <QIcon>
#include <KMessageWidget>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilActivitiesKCMFactory,
                 registerPlugin<ActivityPage>();
                )
K_EXPORT_PLUGIN(PowerDevilActivitiesKCMFactory("powerdevilactivitiesconfig","powerdevil"))

ActivityPage::ActivityPage(QWidget *parent, const QVariantList &args)
    : KCModule(0, parent, args)
    , m_activityConsumer(new KActivities::Consumer(this))
{
    setButtons(Apply | Help);

    /*KAboutData *about =
        new KAboutData("powerdevilactivitiesconfig", "powerdevilactivitiesconfig", ki18n("Activities Power Management Configuration"),
                       "", ki18n("A per-activity configurator of KDE Power Management System"),
                       KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
                       ki18n("From this module, you can fine tune power management settings for each of your activities."));

    about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
                     "http://drfav.wordpress.com");

    setAboutData(about);*/

    // Build the UI
    m_tabWidget = new QTabWidget();
    QVBoxLayout *lay = new QVBoxLayout();

    // Message widget
    m_messageWidget = new KMessageWidget(i18n("The activity service is running with bare functionalities.\n"
                                                          "Names and icons of the activities might not be available."));
    m_messageWidget.data()->setMessageType(KMessageWidget::Warning);
    m_messageWidget.data()->hide();

    lay->addWidget(m_messageWidget.data());
    lay->addWidget(m_tabWidget);
    setLayout(lay);

    onActivityServiceStatusChanged(m_activityConsumer->serviceStatus());
    connect(m_activityConsumer, &KActivities::Consumer::serviceStatusChanged, this, &ActivityPage::onActivityServiceStatusChanged);

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, SIGNAL(serviceRegistered(QString)), this, SLOT(onServiceRegistered(QString)));
    connect(watcher, SIGNAL(serviceUnregistered(QString)), this, SLOT(onServiceUnregistered(QString)));

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }
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

void ActivityPage::onActivityServiceStatusChanged(KActivities::Consumer::ServiceStatus status)
{
    switch (status) {
        case KActivities::Consumer::Unknown: // fall through
        case KActivities::Consumer::NotRunning:
            // Create error overlay, if not present
            if (m_errorOverlay.isNull()) {
                m_errorOverlay = new ErrorOverlay(this, i18n("The activity service is not running.\n"
                                                             "It is necessary to have the activity manager running "
                                                             "to configure activity-specific power management behavior."),
                                                  this);
            }
            break;
        case KActivities::Consumer::Running:
            if (m_previousServiceStatus != KActivities::Consumer::Running) {

                if (!m_errorOverlay.isNull()) {
                    m_errorOverlay.data()->deleteLater();
                    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
                        onServiceRegistered("org.kde.Solid.PowerManagement");
                    } else {
                        onServiceUnregistered("org.kde.Solid.PowerManagement");
                    }
                }

                populateTabs();
            }

            if (m_messageWidget.data()->isVisible()) {
                m_messageWidget.data()->hide();
            }

            break;
    }

    m_previousServiceStatus = status;
}

void ActivityPage::populateTabs()
{
    if (m_activityConsumer->serviceStatus() != KActivities::Consumer::Running) {
        return;
    }

    foreach (const QString &activity, m_activityConsumer->activities()) {
        KActivities::Info *info = new KActivities::Info(activity, this);
        QString icon = info->icon();
        QString name = info->name();
        qCDebug(POWERDEVIL) << activity << info->isValid() << info->availability();

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

        m_tabWidget->addTab(scrollArea, QIcon::fromTheme(icon), name);
    }
}

void ActivityPage::defaults()
{
    KCModule::defaults();
}

void ActivityPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

    if (!m_errorOverlay.isNull()) {
        m_errorOverlay.data()->deleteLater();
    }

    onActivityServiceStatusChanged(m_activityConsumer->serviceStatus());
}

void ActivityPage::onServiceUnregistered(const QString& service)
{
    Q_UNUSED(service);

    if (!m_errorOverlay.isNull()) {
        return;
    }

    m_errorOverlay = new ErrorOverlay(this, i18n("The Power Management Service appears not to be running.\n"
                                                 "This can be solved by starting or scheduling it inside \"Startup and Shutdown\""),
                                      this);
}

#include "activitypage.moc"
