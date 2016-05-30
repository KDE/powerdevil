/***************************************************************************
 *   Copyright (C) 2008-2010 by Dario Freddi <drf@kde.org>                 *
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

#include "EditPage.h"

#include "actioneditwidget.h"
#include "ErrorOverlay.h"

#include <powerdevilactionconfig.h>
#include <powerdevilprofilegenerator.h>

#include <powerdevil_debug.h>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusServiceWatcher>

#include <KConfigGroup>
#include <QDebug>
#include <KMessageBox>
#include <KAboutData>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KServiceTypeTrader>
#include <KLocalizedString>
#include <KRun>

#include <Solid/Battery>
#include <Solid/Device>
#include <Solid/PowerManagement>

K_PLUGIN_FACTORY(PowerDevilProfilesKCMFactory,
                 registerPlugin<EditPage>();
                )

EditPage::EditPage(QWidget *parent, const QVariantList &args)
        : KCModule(0, parent, args)
{
    setButtons(Apply | Help | Default);

//     KAboutData *about =
//         new KAboutData("powerdevilprofilesconfig", "powerdevilprofilesconfig", ki18n("Power Profiles Configuration"),
//                        "", ki18n("A profile configurator for KDE Power Management System"),
//                        KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
//                        ki18n("From this module, you can manage KDE Power Management System's power profiles, by tweaking "
//                              "existing ones or creating new ones."));
//
//     about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
//                      "http://drfav.wordpress.com");
//
//     setAboutData(about);

    setupUi(this);

    m_profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig | KConfig::CascadeConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
        // Use the generator
        QSet <Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();
        PowerDevil::ProfileGenerator::generateProfiles(
            methods.contains(Solid::PowerManagement::SuspendState),
            methods.contains(Solid::PowerManagement::HibernateState)
        );
        m_profilesConfig->reparseConfiguration();
    }

    qCDebug(POWERDEVIL) << m_profilesConfig.data()->groupList() << m_profilesConfig.data()->entryMap().keys();

    // Create widgets for each profile
    ActionEditWidget *editWidget = new ActionEditWidget("AC", tabWidget);
    m_editWidgets.insert("AC", editWidget);
    acWidgetLayout->addWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));

    editWidget = new ActionEditWidget("Battery", tabWidget);
    m_editWidgets.insert("Battery", editWidget);
    batteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));

    editWidget = new ActionEditWidget("LowBattery", tabWidget);
    m_editWidgets.insert("LowBattery", editWidget);
    lowBatteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, SIGNAL(serviceRegistered(QString)), this, SLOT(onServiceRegistered(QString)));
    connect(watcher, SIGNAL(serviceUnregistered(QString)), this, SLOT(onServiceUnregistered(QString)));

    bool hasBattery = false;
    Q_FOREACH(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply() && (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery)) {
            hasBattery = true;
            break;
        }
    }

    if (!hasBattery) {
        tabWidget->setTabEnabled(1, false);
        tabWidget->setTabEnabled(2, false);
        tabWidget->tabBar()->hide();
    }

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }
}

void EditPage::onChanged(bool value)
{
    ActionEditWidget *editWidget = qobject_cast< ActionEditWidget* >(sender());
    if (!editWidget) {
        return;
    }

    m_profileEdited[editWidget->configName()] = value;

    if (value) {
        Q_EMIT changed(true);
    }

    checkAndEmitChanged();
}

void EditPage::load()
{
    qCDebug(POWERDEVIL) << "Loading routine called";
    for (QHash< QString, ActionEditWidget* >::const_iterator i = m_editWidgets.constBegin();
         i != m_editWidgets.constEnd(); ++i) {
        i.value()->load();

        m_profileEdited[i.value()->configName()] = false;
    }
}

void EditPage::save()
{
    for (auto it = m_editWidgets.constBegin(); it != m_editWidgets.constEnd(); ++it) {
        (*it)->save();
    }

    notifyDaemon();

    Q_EMIT changed(false);
}

void EditPage::notifyDaemon()
{
    QDBusConnection::sessionBus().asyncCall(
        QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("refreshStatus")
        )
    );
}

void EditPage::restoreDefaultProfiles()
{
    // Confirm
    int ret = KMessageBox::warningContinueCancel(this, i18n("The KDE Power Management System will now generate a set of defaults "
                                                            "based on your computer's capabilities. This will also erase "
                                                            "all existing modifications you made. "
                                                            "Are you sure you want to continue?"), i18n("Restore Default Profiles"));
    if (ret == KMessageBox::Continue) {
        qCDebug(POWERDEVIL) << "Restoring defaults.";
        QSet <Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();
        PowerDevil::ProfileGenerator::generateProfiles(
            methods.contains(Solid::PowerManagement::SuspendState),
            methods.contains(Solid::PowerManagement::HibernateState)
        );

        load();

        notifyDaemon();
    }
}

void EditPage::openUrl(const QString &url)
{
    new KRun(QUrl(url), this);
}

void EditPage::defaults()
{
    restoreDefaultProfiles();
}

void EditPage::checkAndEmitChanged()
{
    bool value = false;
    for (QHash< QString, bool >::const_iterator i = m_profileEdited.constBegin();
         i != m_profileEdited.constEnd(); ++i) {
        if (i.value()) {
            value = i.value();
        }
    }

    Q_EMIT changed(value);
}

void EditPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

    QDBusPendingCallWatcher *currentProfileWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(
        QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("currentProfile")
        )
    ), this);

    QObject::connect(currentProfileWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QString> reply = *watcher;

        if (!reply.isError()) {
            const QString &currentProfile = reply.value();
            if (currentProfile == QLatin1String("Battery")) {
                tabWidget->setCurrentIndex(1);
            } else if (currentProfile == QLatin1String("LowBattery")) {
                tabWidget->setCurrentIndex(2);
            }
        }

        watcher->deleteLater();
    });

    if (!m_errorOverlay.isNull()) {
        m_errorOverlay.data()->deleteLater();
    }
}

void EditPage::onServiceUnregistered(const QString& service)
{
    Q_UNUSED(service);

    if (!m_errorOverlay.isNull()) {
        m_errorOverlay.data()->deleteLater();
    }

    m_errorOverlay = new ErrorOverlay(this, i18n("The Power Management Service appears not to be running.\n"
                                                 "This can be solved by starting or scheduling it inside \"Startup and Shutdown\""),
                                      this);
}

#include "EditPage.moc"
