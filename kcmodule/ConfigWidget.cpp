/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
 *   Copyright (C) 2008 by Lukas Appelhans <boom1992@kdemod.ath.cx>        *
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *   Copyright (C) 2007 by Luka Renko <lure@kubuntu.org>                   *
 *   Copyright (C) 2008 by Thomas Gillespie <tomjamesgillespie@gmail.com>  *
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

#include "ConfigWidget.h"

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>

ConfigWidget::ConfigWidget(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    fillUi();
}

void ConfigWidget::fillUi()
{
    PoweredIdleCombo->addItem(i18n("Do nothing"), (int) None);
    PoweredIdleCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    BatteryCriticalCombo->addItem(i18n("Do nothing"), (int) None);
    BatteryCriticalCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    BatteryIdleCombo->addItem(i18n("Do nothing"), (int) None);
    BatteryIdleCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedACCombo->addItem(i18n("Do nothing"), (int) None);
    laptopClosedACCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedACCombo->addItem(i18n("Lock Screen"), (int) Lock);
    laptopClosedBatteryCombo->addItem(i18n("Do nothing"), (int) None);
    laptopClosedBatteryCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedBatteryCombo->addItem(i18n("Lock Screen"), (int) Lock);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods | Solid::Control::PowerManager::ToDisk) {
        PoweredIdleCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        BatteryCriticalCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        BatteryIdleCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        laptopClosedACCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        laptopClosedBatteryCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
    }

    if (methods | Solid::Control::PowerManager::ToRam) {
        PoweredIdleCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        BatteryCriticalCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        BatteryIdleCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        laptopClosedACCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        laptopClosedBatteryCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
    }

    if (methods | Solid::Control::PowerManager::Standby) {
        PoweredIdleCombo->addItem(i18n("Standby"), (int) Standby);
        BatteryCriticalCombo->addItem(i18n("Standby"), (int) Standby);
        BatteryIdleCombo->addItem(i18n("Standby"), (int) Standby);
        laptopClosedACCombo->addItem(i18n("Standby"), (int) Standby);
        laptopClosedBatteryCombo->addItem(i18n("Standby"), (int) Standby);
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies | Solid::Control::PowerManager::Performance) {
        PoweredFreqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
        BatteryFreqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
    }

    if (policies | Solid::Control::PowerManager::OnDemand) {
        PoweredFreqCombo->addItem(i18n("Dynamic (aggressive)"), (int) Solid::Control::PowerManager::OnDemand);
        BatteryFreqCombo->addItem(i18n("Dynamic (aggressive)"), (int) Solid::Control::PowerManager::OnDemand);
    }

    if (policies | Solid::Control::PowerManager::Conservative) {
        PoweredFreqCombo->addItem(i18n("Dynamic (less aggressive)"), (int) Solid::Control::PowerManager::Conservative);
        BatteryFreqCombo->addItem(i18n("Dynamic (less aggressive)"), (int) Solid::Control::PowerManager::Conservative);
    }

    if (policies | Solid::Control::PowerManager::Powersave) {
        PoweredFreqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
        BatteryFreqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
    }

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimOnIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(notificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(warningNotificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(warningSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(PoweredBrightnessSlider, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(PoweredOffDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(PoweredDisplayIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(PoweredIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(PoweredIdleCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(PoweredFreqCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(BatteryBrightnessSlider, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(BatteryOffDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(BatteryDisplayIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(BatteryIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(BatteryIdleCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(BatteryFreqCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(laptopClosedBatteryCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(laptopClosedACCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(PoweredOffDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
    connect(BatteryOffDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
}

void ConfigWidget::load()
{
    lockScreenOnResume->setChecked(PowerDevilSettings::configLockScreen());
    dimDisplayOnIdle->setChecked(PowerDevilSettings::dimOnIdle());
    dimOnIdleTime->setValue(PowerDevilSettings::dimOnIdleTime());
    notificationsBox->setChecked(PowerDevilSettings::enableNotifications());
    warningNotificationsBox->setChecked(PowerDevilSettings::enableWarningNotifications());

    lowSpin->setValue(PowerDevilSettings::batteryLowLevel());
    warningSpin->setValue(PowerDevilSettings::batteryWarningLevel());
    criticalSpin->setValue(PowerDevilSettings::batteryCriticalLevel());

    PoweredBrightnessSlider->setValue(PowerDevilSettings::aCBrightness());
    PoweredOffDisplayWhenIdle->setChecked(PowerDevilSettings::aCOffDisplayWhenIdle());
    PoweredDisplayIdleTime->setValue(PowerDevilSettings::aCDisplayIdle());
    PoweredIdleTime->setValue(PowerDevilSettings::aCIdle());
    PoweredIdleCombo->setCurrentIndex(PoweredIdleCombo->findData(PowerDevilSettings::aCIdleAction()));
    PoweredFreqCombo->setCurrentIndex(PoweredFreqCombo->findData(PowerDevilSettings::aCCpuPolicy()));

    BatteryBrightnessSlider->setValue(PowerDevilSettings::batBrightness());
    BatteryOffDisplayWhenIdle->setChecked(PowerDevilSettings::batOffDisplayWhenIdle());
    BatteryDisplayIdleTime->setValue(PowerDevilSettings::batDisplayIdle());
    BatteryIdleTime->setValue(PowerDevilSettings::batIdle());
    BatteryIdleCombo->setCurrentIndex(BatteryIdleCombo->findData(PowerDevilSettings::batIdleAction()));
    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batLowAction()));
    BatteryFreqCombo->setCurrentIndex(BatteryFreqCombo->findData(PowerDevilSettings::batCpuPolicy()));

    laptopClosedBatteryCombo->setCurrentIndex(laptopClosedBatteryCombo->findData(PowerDevilSettings::batLidAction()));
    laptopClosedACCombo->setCurrentIndex(laptopClosedACCombo->findData(PowerDevilSettings::aCLidAction()));

    enableBoxes();
}

void ConfigWidget::save()
{
    PowerDevilSettings::setConfigLockScreen(lockScreenOnResume->isChecked());
    PowerDevilSettings::setDimOnIdle(dimDisplayOnIdle->isChecked());
    PowerDevilSettings::setDimOnIdleTime(dimOnIdleTime->value());
    PowerDevilSettings::setEnableNotifications(notificationsBox->isChecked());
    PowerDevilSettings::setEnableWarningNotifications(warningNotificationsBox->isChecked());

    PowerDevilSettings::setBatteryLowLevel(lowSpin->value());
    PowerDevilSettings::setBatteryWarningLevel(warningSpin->value());
    PowerDevilSettings::setBatteryCriticalLevel(criticalSpin->value());

    PowerDevilSettings::setACBrightness(PoweredBrightnessSlider->value());
    PowerDevilSettings::setACOffDisplayWhenIdle(PoweredOffDisplayWhenIdle->isChecked());
    PowerDevilSettings::setACDisplayIdle(PoweredDisplayIdleTime->value());
    PowerDevilSettings::setACIdle(PoweredIdleTime->value());
    PowerDevilSettings::setACIdleAction(PoweredIdleCombo->itemData(PoweredIdleCombo->currentIndex()).toInt());
    PowerDevilSettings::setACCpuPolicy(PoweredFreqCombo->itemData(PoweredFreqCombo->currentIndex()).toInt());

    PowerDevilSettings::setBatBrightness(BatteryBrightnessSlider->value());
    PowerDevilSettings::setBatOffDisplayWhenIdle(BatteryOffDisplayWhenIdle->isChecked());
    PowerDevilSettings::setBatDisplayIdle(BatteryDisplayIdleTime->value());
    PowerDevilSettings::setBatIdle(BatteryIdleTime->value());
    PowerDevilSettings::setBatIdleAction(BatteryIdleCombo->itemData(BatteryIdleCombo->currentIndex()).toInt());
    PowerDevilSettings::setBatCpuPolicy(BatteryFreqCombo->itemData(BatteryFreqCombo->currentIndex()).toInt());
    PowerDevilSettings::setBatLowAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());


    PowerDevilSettings::setBatLidAction(laptopClosedBatteryCombo->itemData(laptopClosedBatteryCombo->currentIndex()).toInt());

    PowerDevilSettings::setACLidAction(laptopClosedACCombo->itemData(laptopClosedACCombo->currentIndex()).toInt());


    PowerDevilSettings::self()->writeConfig();
}

void ConfigWidget::emitChanged()
{
    emit changed(true);
}

void ConfigWidget::enableBoxes()
{
    BatteryDisplayIdleTime->setEnabled(BatteryOffDisplayWhenIdle->isChecked());
    PoweredDisplayIdleTime->setEnabled(PoweredOffDisplayWhenIdle->isChecked());
}

#include "ConfigWidget.moc"
