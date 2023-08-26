/*
 *   SPDX-FileCopyrightText: 2011 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "activitypage.h"

#include "activitywidget.h"

#include <powerdevil_debug.h>

#include <ErrorOverlay.h>

#include <QScrollArea>
#include <QVBoxLayout>

#include <QTabWidget>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>

#include <KAboutData>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QDebug>
#include <QIcon>

K_PLUGIN_CLASS_WITH_JSON(ActivityPage, "kcm_powerdevilactivitiesconfig.json")

ActivityPage::ActivityPage(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_activityConsumer(new KActivities::Consumer(this))
{
    setButtons(Apply | Help);

    QVBoxLayout *lay = new QVBoxLayout();

    // Message widget
    m_messageWidget = new KMessageWidget(i18n("The activity service is running with bare functionalities.\n"
                                              "Names and icons of the activities might not be available."),
                                         widget());
    m_messageWidget->setMessageType(KMessageWidget::Warning);
    m_messageWidget->hide();

    // Tab widget (must set size here since tabs are loaded after initial layout size is calculated)
    m_tabWidget = new QTabWidget(widget());
    m_tabWidget->setMinimumSize(400, 200);

    lay->addWidget(m_messageWidget);
    lay->addWidget(m_tabWidget);
    widget()->setLayout(lay);

    onActivityServiceStatusChanged(m_activityConsumer->serviceStatus());
    connect(m_activityConsumer, &KActivities::Consumer::serviceStatusChanged, this, &ActivityPage::onActivityServiceStatusChanged);

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &ActivityPage::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &ActivityPage::onServiceUnregistered);

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
    for (ActivityWidget *widget : qAsConst(m_activityWidgets)) {
        widget->load();
    }

    setNeedsSave(false);
}

void ActivityPage::save()
{
    for (ActivityWidget *widget : qAsConst(m_activityWidgets)) {
        widget->save();
    }

    setNeedsSave(false);

    // Ask to refresh status
    QDBusMessage call =
        QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement", "org.kde.Solid.PowerManagement", "refreshStatus");

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
        if (!m_errorOverlay) {
            m_errorOverlay = new ErrorOverlay(widget(),
                                              i18n("The activity service is not running.\n"
                                                   "It is necessary to have the activity manager running "
                                                   "to configure activity-specific power management behavior."),
                                              widget());
        }
        break;
    case KActivities::Consumer::Running:
        if (m_previousServiceStatus != KActivities::Consumer::Running) {
            if (m_errorOverlay) {
                m_errorOverlay->deleteLater();
                m_errorOverlay = nullptr;
                if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
                    onServiceRegistered("org.kde.Solid.PowerManagement");
                } else {
                    onServiceUnregistered("org.kde.Solid.PowerManagement");
                }
            }

            populateTabs();
        }

        if (m_messageWidget->isVisible()) {
            m_messageWidget->hide();
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

    int index = 0;
    const QStringList activities = m_activityConsumer->activities();
    for (const QString &activity : activities) {
        KActivities::Info *info = new KActivities::Info(activity, this);
        const QString icon = info->icon();
        const QString name = info->name();
        qCDebug(POWERDEVIL) << activity << info->isValid() << info->availability();

        QScrollArea *scrollArea = new QScrollArea(widget());
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setFrameShadow(QFrame::Plain);
        scrollArea->setLineWidth(0);
        scrollArea->setWidgetResizable(true);

        ActivityWidget *activityWidget = new ActivityWidget(activity);
        scrollArea->setWidget(activityWidget);

        activityWidget->load();
        m_activityWidgets.append(activityWidget);

        connect(activityWidget, &ActivityWidget::changed, this, &KCModule::setNeedsSave);
        if (!icon.isEmpty()) {
            m_tabWidget->addTab(scrollArea, QIcon::fromTheme(icon), name);
        } else {
            m_tabWidget->addTab(scrollArea, name);
        }

        if (m_activityConsumer->currentActivity() == activity) {
            m_tabWidget->setCurrentIndex(index);
        }

        ++index;
    }
}

void ActivityPage::defaults()
{
    KCModule::defaults();
}

void ActivityPage::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
        m_errorOverlay = nullptr;
    }
}

void ActivityPage::onServiceUnregistered(const QString &service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        return;
    }

    m_errorOverlay = new ErrorOverlay(widget(), i18n("The Power Management Service appears not to be running."), widget());
}

#include "activitypage.moc"

#include "moc_activitypage.cpp"
