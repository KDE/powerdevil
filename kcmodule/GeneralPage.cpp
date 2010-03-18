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

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>
#include <solid/battery.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KNotifyConfigWidget>

GeneralPage::GeneralPage(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    m_profilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    fillUi();
}

GeneralPage::~GeneralPage()
{
}

void GeneralPage::fillUi()
{
    int batteryCount = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Solid::Device dev = device;
        Solid::Battery *b = qobject_cast<Solid::Battery*> (dev.asDeviceInterface(Solid::DeviceInterface::Battery));
        if(b->type() != Solid::Battery::PrimaryBattery) {
            continue;
        }
        ++batteryCount;
    }

    reloadAvailableProfiles();

    tabWidget->setTabIcon(0, KIcon("preferences-other"));
    tabWidget->setTabIcon(1, KIcon("battery"));

    issueIcon->setPixmap(KIcon("dialog-warning").pixmap(32, 32));
    issueIcon->setVisible(false);
    issueText->setVisible(false);

    BatteryCriticalCombo->addItem(KIcon("dialog-cancel"), i18n("Do nothing"), (int) None);
    BatteryCriticalCombo->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (int) Shutdown);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods & Solid::Control::PowerManager::ToDisk) {
        BatteryCriticalCombo->addItem(KIcon("system-suspend-hibernate"), i18n("Suspend to Disk"), (int) S2Disk);
    }

    if (methods & Solid::Control::PowerManager::ToRam) {
        BatteryCriticalCombo->addItem(KIcon("system-suspend"), i18n("Suspend to RAM"), (int) S2Ram);
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        BatteryCriticalCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
    }

    notificationsButton->setIcon(KIcon("preferences-desktop-notification"));

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(enableDPMSBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(suspendWaitTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(notificationsButton, SIGNAL(clicked()), SLOT(configureNotifications()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(warningSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(acProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(lowProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(warningProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(batteryProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    // Disable stuff, eventually
    if (batteryCount == 0) {
        batteryProfile->setEnabled(false);
        lowProfile->setEnabled(false);
        warningProfile->setEnabled(false);
        tabWidget->setTabEnabled(1, false);
    }
}

void GeneralPage::load()
{
    lockScreenOnResume->setChecked(PowerDevilSettings::configLockScreen());
    enableDPMSBox->setChecked(PowerDevilSettings::manageDPMS());
    if (PowerDevilSettings::waitBeforeSuspending()) {
        suspendWaitTime->setValue(PowerDevilSettings::waitBeforeSuspendingTime());
    } else {
        suspendWaitTime->setValue(0);
    }

    lowSpin->setValue(PowerDevilSettings::batteryLowLevel());
    warningSpin->setValue(PowerDevilSettings::batteryWarningLevel());
    criticalSpin->setValue(PowerDevilSettings::batteryCriticalLevel());

    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batLowAction()));

    acProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::aCProfile()));
    lowProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::lowProfile()));
    warningProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::warningProfile()));
    batteryProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::batteryProfile()));
}

void GeneralPage::configureNotifications()
{
    KNotifyConfigWidget::configure(this, "powerdevil");
}

void GeneralPage::save()
{
    PowerDevilSettings::setConfigLockScreen(lockScreenOnResume->isChecked());
    PowerDevilSettings::setManageDPMS(enableDPMSBox->isChecked());
    PowerDevilSettings::setWaitBeforeSuspending(suspendWaitTime->value() != 0);
    PowerDevilSettings::setWaitBeforeSuspendingTime(suspendWaitTime->value());

    PowerDevilSettings::setBatteryLowLevel(lowSpin->value());
    PowerDevilSettings::setBatteryWarningLevel(warningSpin->value());
    PowerDevilSettings::setBatteryCriticalLevel(criticalSpin->value());

    PowerDevilSettings::setBatLowAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());

    PowerDevilSettings::setACProfile(acProfile->currentText());
    PowerDevilSettings::setLowProfile(lowProfile->currentText());
    PowerDevilSettings::setWarningProfile(warningProfile->currentText());
    PowerDevilSettings::setBatteryProfile(batteryProfile->currentText());

    PowerDevilSettings::self()->writeConfig();
}

void GeneralPage::emitChanged()
{
    emit changed(true);
}

void GeneralPage::enableIssue(bool enable)
{
    issueIcon->setVisible(enable);
    issueText->setVisible(enable);
}

void GeneralPage::reloadAvailableProfiles()
{
    m_profilesConfig->reparseConfiguration();

    acProfile->clear();
    batteryProfile->clear();
    lowProfile->clear();
    warningProfile->clear();

    if (m_profilesConfig->groupList().isEmpty()) {
        kDebug() << "No available profiles!";
        return;
    }

    foreach(const QString &ent, m_profilesConfig->groupList()) {
        KConfigGroup *group = new KConfigGroup(m_profilesConfig, ent);

        acProfile->addItem(KIcon(group->readEntry("iconname")), ent);
        batteryProfile->addItem(KIcon(group->readEntry("iconname")), ent);
        lowProfile->addItem(KIcon(group->readEntry("iconname")), ent);
        warningProfile->addItem(KIcon(group->readEntry("iconname")), ent);
        delete group;
    }

    acProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::aCProfile()));
    lowProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::lowProfile()));
    warningProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::warningProfile()));
    batteryProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::batteryProfile()));

}

#include "GeneralPage.moc"
