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

#include "GeneralPage.h"

#include "ErrorOverlay.h"
#include "PowerDevilSettings.h"

#include "actions/bundled/suspendsession.h"

#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/Battery>
#include <Solid/PowerManagement>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusServiceWatcher>

#include <KNotifyConfigWidget>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KAboutData>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilGeneralKCMFactory,
                 registerPlugin<GeneralPage>();
                )

typedef QMap< QString, QString > StringStringMap;
Q_DECLARE_METATYPE(StringStringMap)

GeneralPage::GeneralPage(QWidget *parent, const QVariantList &args)
        : KCModule(0, parent, args)
{
    qDBusRegisterMetaType< StringStringMap >();

    setButtons(Apply | Help);

//     KAboutData *about =
//         new KAboutData("powerdevilglobalconfig", "powerdevilglobalconfig", ki18n("Global Power Management Configuration"),
//                        "", ki18n("A global power management configurator for KDE Power Management System"),
//                        KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
//                        ki18n("From this module, you can configure the main Power Management daemon, assign profiles to "
//                              "states, and do some advanced fine tuning on battery handling"));
//
//     about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
//                      "http://drfav.wordpress.com");
//
//     setAboutData(about);

    setupUi(this);

    fillUi();

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

GeneralPage::~GeneralPage()
{
}

void GeneralPage::fillUi()
{
    bool hasBattery = false;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if(b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery) {
            hasBattery = true;
            break;
        }
    }

    QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

    BatteryCriticalCombo->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), PowerDevil::BundledActions::SuspendSession::None);
    if (methods.contains(Solid::PowerManagement::SuspendState)) {
        BatteryCriticalCombo->addItem(QIcon::fromTheme("system-suspend"), i18n("Sleep"), PowerDevil::BundledActions::SuspendSession::ToRamMode);
    }
    if (methods.contains(Solid::PowerManagement::HibernateState)) {
        BatteryCriticalCombo->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), PowerDevil::BundledActions::SuspendSession::ToDiskMode);
    }
    BatteryCriticalCombo->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shutdown"), PowerDevil::BundledActions::SuspendSession::ShutdownMode);

    notificationsButton->setIcon(QIcon::fromTheme("preferences-desktop-notification"));

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(changed()));
    connect(doNotInhibitLid, SIGNAL(stateChanged(int)), SLOT(changed()));

    connect(notificationsButton, SIGNAL(clicked()), SLOT(configureNotifications()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(changed()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));

    // Disable stuff, eventually
    if (!hasBattery) {
        batteryLevelsLabel->hide();

        BatteryCriticalLabel->hide();
        BatteryCriticalCombo->hide();
        lowLabel->hide();
        lowSpin->hide();
        criticalLabel->hide();
        criticalSpin->hide();
    }
}

void GeneralPage::load()
{
    lockScreenOnResume->setChecked(PowerDevilSettings::configLockScreen());
    doNotInhibitLid->setChecked(PowerDevilSettings::doNotInhibitOnLidClose());

    lowSpin->setValue(PowerDevilSettings::batteryLowLevel());
    criticalSpin->setValue(PowerDevilSettings::batteryCriticalLevel());

    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batteryCriticalAction()));
}

void GeneralPage::configureNotifications()
{
    KNotifyConfigWidget::configure(this, "powerdevil");
}

void GeneralPage::save()
{
    PowerDevilSettings::setConfigLockScreen(lockScreenOnResume->isChecked());
    PowerDevilSettings::setDoNotInhibitOnLidClose(doNotInhibitLid->isChecked());

    PowerDevilSettings::setBatteryLowLevel(lowSpin->value());
    PowerDevilSettings::setBatteryCriticalLevel(criticalSpin->value());

    PowerDevilSettings::setBatteryCriticalAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());

    PowerDevilSettings::self()->writeConfig();

    // Notify Daemon
    QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                       "org.kde.Solid.PowerManagement", "refreshStatus");

    // Perform call
    QDBusConnection::sessionBus().asyncCall(call);

    // And now we are set with no change
    emit changed(false);
}

void GeneralPage::defaults()
{
    KCModule::defaults();
}

void GeneralPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

    if (!m_errorOverlay.isNull()) {
        m_errorOverlay.data()->deleteLater();
    }
}

void GeneralPage::onServiceUnregistered(const QString& service)
{
    Q_UNUSED(service);

    if (!m_errorOverlay.isNull()) {
        m_errorOverlay.data()->deleteLater();
    }

    m_errorOverlay = new ErrorOverlay(this, i18n("The Power Management Service appears not to be running.\n"
                                                 "This can be solved by starting or scheduling it inside \"Startup and Shutdown\""),
                                      this);
}

#include "GeneralPage.moc"
