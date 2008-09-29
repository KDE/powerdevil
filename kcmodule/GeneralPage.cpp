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

#include "GeneralPage.h"

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KNotifyConfigWidget>

GeneralPage::GeneralPage(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    fillUi();
}

GeneralPage::~GeneralPage()
{
}

void GeneralPage::fillUi()
{
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
        BatteryCriticalCombo->addItem(KIcon("system-suspend"), i18n("Suspend to Ram"), (int) S2Ram);
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        BatteryCriticalCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
    }

    notificationsButton->setIcon(KIcon("preferences-desktop-notification"));

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimOnIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(notificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(warningNotificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

    connect(notificationsButton, SIGNAL(clicked()), SLOT(configureNotifications()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(warningSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
}

void GeneralPage::load()
{
    lockScreenOnResume->setChecked(PowerDevilSettings::configLockScreen());
    dimDisplayOnIdle->setChecked(PowerDevilSettings::dimOnIdle());
    dimOnIdleTime->setValue(PowerDevilSettings::dimOnIdleTime());
    notificationsBox->setChecked(PowerDevilSettings::enableNotifications());
    warningNotificationsBox->setChecked(PowerDevilSettings::enableWarningNotifications());

    lowSpin->setValue(PowerDevilSettings::batteryLowLevel());
    warningSpin->setValue(PowerDevilSettings::batteryWarningLevel());
    criticalSpin->setValue(PowerDevilSettings::batteryCriticalLevel());

    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batLowAction()));

    enableBoxes();
}

void GeneralPage::configureNotifications()
{
    KNotifyConfigWidget::configure(this, "powerdevil");
}

void GeneralPage::save()
{
    PowerDevilSettings::setConfigLockScreen(lockScreenOnResume->isChecked());
    PowerDevilSettings::setDimOnIdle(dimDisplayOnIdle->isChecked());
    PowerDevilSettings::setDimOnIdleTime(dimOnIdleTime->value());
    PowerDevilSettings::setEnableNotifications(notificationsBox->isChecked());
    PowerDevilSettings::setEnableWarningNotifications(warningNotificationsBox->isChecked());

    PowerDevilSettings::setBatteryLowLevel(lowSpin->value());
    PowerDevilSettings::setBatteryWarningLevel(warningSpin->value());
    PowerDevilSettings::setBatteryCriticalLevel(criticalSpin->value());

    PowerDevilSettings::setBatLowAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());

    PowerDevilSettings::self()->writeConfig();
}

void GeneralPage::emitChanged()
{
    emit changed(true);
}

void GeneralPage::enableBoxes()
{
    dimOnIdleTime->setEnabled(dimDisplayOnIdle->isChecked());
}

void GeneralPage::enableIssue(bool enable)
{
    issueIcon->setVisible(enable);
    issueText->setVisible(enable);
}

#include "GeneralPage.moc"
