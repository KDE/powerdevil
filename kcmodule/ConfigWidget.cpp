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
    PoweredIdleCombo->addItem(i18n("Do nothing"));
    PoweredIdleCombo->addItem(i18n("Shutdown"));
    BatteryCriticalCombo->addItem(i18n("Do nothing"));
    BatteryCriticalCombo->addItem(i18n("Shutdown"));
    BatteryIdleCombo->addItem(i18n("Do nothing"));
    BatteryIdleCombo->addItem(i18n("Shutdown"));

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if(methods | Solid::Control::PowerManager::ToDisk)
    {
        PoweredIdleCombo->addItem(i18n("Suspend to Disk"));
        BatteryCriticalCombo->addItem(i18n("Suspend to Disk"));
        BatteryIdleCombo->addItem(i18n("Suspend to Disk"));
    }
    if(methods | Solid::Control::PowerManager::ToRam)
    {
        PoweredIdleCombo->addItem(i18n("Suspend to Ram"));
        BatteryCriticalCombo->addItem(i18n("Suspend to Ram"));
        BatteryIdleCombo->addItem(i18n("Suspend to Ram"));
    }
    if(methods | Solid::Control::PowerManager::Standby)
    {
        PoweredIdleCombo->addItem(i18n("Standby"));
        BatteryCriticalCombo->addItem(i18n("Standby"));
        BatteryIdleCombo->addItem(i18n("Standby"));
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if(policies | Solid::Control::PowerManager::Performance)
    {
        PoweredFreqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
        BatteryFreqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
    }
    if(policies | Solid::Control::PowerManager::OnDemand)
    {
        PoweredFreqCombo->addItem(i18n("Dynamic (aggressive)"), (int) Solid::Control::PowerManager::OnDemand);
        BatteryFreqCombo->addItem(i18n("Dynamic (aggressive)"), (int) Solid::Control::PowerManager::OnDemand);
    }
    if(policies | Solid::Control::PowerManager::Conservative)
    {
        PoweredFreqCombo->addItem(i18n("Dynamic (less aggressive)"), (int) Solid::Control::PowerManager::Conservative);
        BatteryFreqCombo->addItem(i18n("Dynamic (less aggressive)"), (int) Solid::Control::PowerManager::Conservative);
    }
    if(policies | Solid::Control::PowerManager::Powersave)
    {
        PoweredFreqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
        BatteryFreqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
    }

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

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
    PoweredBrightnessSlider->setValue(PowerDevilSettings::aCBrightness());
    PoweredOffDisplayWhenIdle->setChecked(PowerDevilSettings::aCOffDisplayWhenIdle());
    PoweredDisplayIdleTime->setValue(PowerDevilSettings::aCDisplayIdle());
    PoweredIdleTime->setValue(PowerDevilSettings::aCIdle());
    PoweredIdleCombo->setCurrentIndex(PowerDevilSettings::aCIdleAction());
    PoweredFreqCombo->setCurrentIndex(PowerDevilSettings::aCCpuPolicy());
    BatteryBrightnessSlider->setValue(PowerDevilSettings::batBrightness());
    BatteryOffDisplayWhenIdle->setChecked(PowerDevilSettings::batOffDisplayWhenIdle());
    BatteryDisplayIdleTime->setValue(PowerDevilSettings::batDisplayIdle());
    BatteryIdleTime->setValue(PowerDevilSettings::batIdle());
    BatteryIdleCombo->setCurrentIndex(PowerDevilSettings::batIdleAction());
    BatteryCriticalCombo->setCurrentIndex(PowerDevilSettings::batLowAction());
    BatteryFreqCombo->setCurrentIndex(PowerDevilSettings::batCpuPolicy());

    laptopClosedBatteryCombo->setCurrentIndex(PowerDevilSettings::batLidAction());

    laptopClosedACCombo->setCurrentIndex(PowerDevilSettings::aCLidAction());
    
    enableBoxes();
}

void ConfigWidget::save()
{
    PowerDevilSettings::setConfigLockScreen(lockScreenOnResume->isChecked());
    PowerDevilSettings::setDimOnIdle(dimDisplayOnIdle->isChecked());

    PowerDevilSettings::setACBrightness(PoweredBrightnessSlider->value());
    PowerDevilSettings::setACOffDisplayWhenIdle(PoweredOffDisplayWhenIdle->isChecked());
    PowerDevilSettings::setACDisplayIdle(PoweredDisplayIdleTime->value());
    PowerDevilSettings::setACIdle(PoweredIdleTime->value());
    PowerDevilSettings::setACIdleAction(PoweredIdleCombo->currentIndex());
    PowerDevilSettings::setACCpuPolicy(PoweredFreqCombo->currentIndex());

    PowerDevilSettings::setBatBrightness(BatteryBrightnessSlider->value());
    PowerDevilSettings::setBatOffDisplayWhenIdle(BatteryOffDisplayWhenIdle->isChecked());
    PowerDevilSettings::setBatDisplayIdle(BatteryDisplayIdleTime->value());
    PowerDevilSettings::setBatIdle(BatteryIdleTime->value());
    PowerDevilSettings::setBatIdleAction(BatteryIdleCombo->currentIndex());
    PowerDevilSettings::setBatCpuPolicy(BatteryFreqCombo->currentIndex());
    PowerDevilSettings::setBatLowAction(BatteryCriticalCombo->currentIndex());


    PowerDevilSettings::setBatLidAction(laptopClosedBatteryCombo->currentIndex());

    PowerDevilSettings::setACLidAction(laptopClosedACCombo->currentIndex());


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
