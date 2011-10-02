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

#include <daemon/powerdevilactionconfig.h>
#include <daemon/powerdevilprofilegenerator.h>

#include <config-workspace.h>

#include <QtGui/QCheckBox>
#include <QtGui/QFormLayout>
#include <QtGui/QLabel>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusServiceWatcher>

#include <KConfigGroup>
#include <KLineEdit>
#include <KDebug>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KIconButton>
#include <KToolBar>
#include <KAboutData>
#include <KPluginFactory>
#include <KServiceTypeTrader>
#include <KStandardDirs>
#include <KRun>

#include <Solid/Battery>
#include <Solid/Device>

K_PLUGIN_FACTORY(PowerDevilProfilesKCMFactory,
                 registerPlugin<EditPage>();
                )
K_EXPORT_PLUGIN(PowerDevilProfilesKCMFactory("powerdevilprofilesconfig","powerdevil"))

typedef QMap< QString, QString > StringStringMap;
Q_DECLARE_METATYPE(StringStringMap)

EditPage::EditPage(QWidget *parent, const QVariantList &args)
        : KCModule(PowerDevilProfilesKCMFactory::componentData(), parent, args)
{
    qDBusRegisterMetaType< StringStringMap >();

    setButtons(Apply | Help | Default);

    KAboutData *about =
        new KAboutData("powerdevilprofilesconfig", "powerdevilprofilesconfig", ki18n("Power Profiles Configuration"),
                       "", ki18n("A profile configurator for KDE Power Management System"),
                       KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
                       ki18n("From this module, you can manage KDE Power Management System's power profiles, by tweaking "
                             "existing ones or creating new ones."));

    about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
                     "http://drfav.wordpress.com");

    setAboutData(about);

    setupUi(this);

    m_profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
        // Use the generator
        PowerDevil::ProfileGenerator::generateProfiles();
        m_profilesConfig->reparseConfiguration();
    }

    kDebug() << m_profilesConfig.data()->groupList() << m_profilesConfig.data()->entryMap().keys();

    // Create widgets for each profile
    ActionEditWidget *editWidget = new ActionEditWidget("AC");
    m_editWidgets.insert("AC", editWidget);
    acScrollArea->setWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));
    tabWidget->setTabIcon(0, KIcon("battery-charging"));

    editWidget = new ActionEditWidget("Battery");
    m_editWidgets.insert("Battery", editWidget);
    batteryScrollArea->setWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));
    tabWidget->setTabIcon(1, KIcon("battery-060"));

    editWidget = new ActionEditWidget("LowBattery");
    m_editWidgets.insert("LowBattery", editWidget);
    lowBatteryScrollArea->setWidget(editWidget);
    connect(editWidget, SIGNAL(changed(bool)), this, SLOT(onChanged(bool)));
    tabWidget->setTabIcon(2, KIcon("battery-low"));

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, SIGNAL(serviceRegistered(QString)), this, SLOT(onServiceRegistered(QString)));
    connect(watcher, SIGNAL(serviceUnregistered(QString)), this, SLOT(onServiceUnregistered(QString)));

    int batteryCount = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if(b->type() != Solid::Battery::PrimaryBattery && b->type() != Solid::Battery::UpsBattery) {
            continue;
        }
        ++batteryCount;
    }

    if (batteryCount == 0) {
        tabWidget->setTabEnabled(1, false);
        tabWidget->setTabEnabled(2, false);
    }

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }
}

EditPage::~EditPage()
{
}

void EditPage::onChanged(bool value)
{
    ActionEditWidget *editWidget = qobject_cast< ActionEditWidget* >(sender());
    if (!editWidget) {
        return;
    }

    m_profileEdited[editWidget->configName()] = value;

    if (value) {
        emit changed(true);
    }

    checkAndEmitChanged();
}

void EditPage::load()
{
    kDebug() << "Loading routine called";
    for (QHash< QString, ActionEditWidget* >::const_iterator i = m_editWidgets.constBegin();
         i != m_editWidgets.constEnd(); ++i) {
        i.value()->load();

        m_profileEdited[i.value()->configName()] = false;
    }
}

void EditPage::save()
{
    QStringList profiles;

    for (QHash< QString, ActionEditWidget* >::const_iterator i = m_editWidgets.constBegin();
         i != m_editWidgets.constEnd(); ++i) {
        i.value()->save();
        if (m_profileEdited[i.value()->configName()]) {
            profiles.append(i.value()->configName());
        }

        m_profileEdited[i.value()->configName()] = false;
    }
    // Notify the daemon
    notifyDaemon(profiles);

    emit changed(false);
}

void EditPage::notifyDaemon(const QStringList &editedProfiles)
{
    QDBusMessage call;
    if (!editedProfiles.isEmpty()) {
        call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                              "org.kde.Solid.PowerManagement", "currentProfile");
        QDBusPendingReply< QString > reply = QDBusConnection::sessionBus().asyncCall(call);
        reply.waitForFinished();

        if (reply.isValid()) {
            if (editedProfiles.contains(reply.value())) {
                // Ask to reload the profile
                kDebug() << "Active profile edited, reloading profile";
                call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                      "org.kde.Solid.PowerManagement", "reloadCurrentProfile");
            } else {
                // Ask to reparse config
                kDebug() << "Inactive profile edited, reparsing configuration";
                call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                      "org.kde.Solid.PowerManagement", "reparseConfiguration");
            }
        } else {
            kWarning() << "Invalid reply from daemon when asking for current profile!";
            // To be sure, reload profile
            call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                  "org.kde.Solid.PowerManagement", "reloadCurrentProfile");
        }
    } else {
        // Refresh status
        call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                              "org.kde.Solid.PowerManagement", "refreshStatus");
    }

    // Perform call
    QDBusConnection::sessionBus().asyncCall(call);
}

void EditPage::restoreDefaultProfiles()
{
    // Confirm
    int ret = KMessageBox::warningContinueCancel(this, i18n("The KDE Power Management System will now generate a set of defaults "
                                                            "based on your computer's capabilities. This will also erase "
                                                            "all existing modifications you made. "
                                                            "Are you sure you want to continue?"), i18n("Restore Default Profiles"));
    if (ret == KMessageBox::Continue) {
        kDebug() << "Restoring defaults.";
        PowerDevil::ProfileGenerator::generateProfiles();

        load();

        // Notify the daemon
        notifyDaemon();
    }
}

void EditPage::openUrl(const QString &url)
{
    new KRun(KUrl(url), this);
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

    emit changed(value);
}

void EditPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

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
