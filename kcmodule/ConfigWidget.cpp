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

#include <KConfigGroup>
#include <KLineEdit>
#include <KDialog>

ConfigWidget::ConfigWidget(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    m_profilesConfig = new KConfig( "powerdevilprofilesrc", KConfig::SimpleConfig );

    if (m_profilesConfig->groupList().isEmpty())
    {
        // Let's add some basic profiles, huh?

        KConfigGroup *performance = new KConfigGroup(m_profilesConfig, "Performance");

        performance->writeEntry("brightness", 100);
        performance->writeEntry("cpuPolicy", (int) Solid::Control::PowerManager::Performance);
        performance->writeEntry("idleAction", 0);
        performance->writeEntry("idleTime", 50);
        performance->writeEntry("lidAction", 0);
        performance->writeEntry("turnOffIdle", false);
        performance->writeEntry("turnOffIdleTime", 120);

        performance->sync();

        kDebug() << performance->readEntry("brightness");

        delete performance;
    }

    fillUi();
}

void ConfigWidget::fillUi()
{
    idleCombo->addItem(i18n("Do nothing"), (int) None);
    idleCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    BatteryCriticalCombo->addItem(i18n("Do nothing"), (int) None);
    BatteryCriticalCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedCombo->addItem(i18n("Do nothing"), (int) None);
    laptopClosedCombo->addItem(i18n("Shutdown"), (int) Shutdown);
    laptopClosedCombo->addItem(i18n("Lock Screen"), (int) Lock);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods | Solid::Control::PowerManager::ToDisk) {
        idleCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        BatteryCriticalCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
        laptopClosedCombo->addItem(i18n("Suspend to Disk"), (int) S2Disk);
    }

    if (methods | Solid::Control::PowerManager::ToRam) {
        idleCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        BatteryCriticalCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
        laptopClosedCombo->addItem(i18n("Suspend to Ram"), (int) S2Ram);
    }

    if (methods | Solid::Control::PowerManager::Standby) {
        idleCombo->addItem(i18n("Standby"), (int) Standby);
        BatteryCriticalCombo->addItem(i18n("Standby"), (int) Standby);
        laptopClosedCombo->addItem(i18n("Standby"), (int) Standby);
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies | Solid::Control::PowerManager::Performance) {
        freqCombo->addItem(i18n("Performance"), (int) Solid::Control::PowerManager::Performance);
    }

    if (policies | Solid::Control::PowerManager::OnDemand) {
        freqCombo->addItem(i18n("Dynamic (ondemand)"), (int) Solid::Control::PowerManager::OnDemand);
    }

    if (policies | Solid::Control::PowerManager::Conservative) {
        freqCombo->addItem(i18n("Dynamic (conservative)"), (int) Solid::Control::PowerManager::Conservative);
    }

    if (policies | Solid::Control::PowerManager::Powersave) {
        freqCombo->addItem(i18n("Powersave"), (int) Solid::Control::PowerManager::Powersave);
    }

    reloadAvailableProfiles();

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimOnIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(notificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(warningNotificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(warningSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(brightnessSlider, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(offDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(displayIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(idleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(idleCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(freqCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(offDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(acProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(lowProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(warningProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(batteryProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(profileEditBox, SIGNAL(currentIndexChanged(int)), SLOT(loadProfile()));

    connect(deleteProfile, SIGNAL(clicked()), SLOT(deleteCurrentProfile()));
    connect(newProfile, SIGNAL(clicked()), SLOT(createProfile()));
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

    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batLowAction()));

    acProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::aCProfile()));
    lowProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::lowProfile()));
    warningProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::warningProfile()));
    batteryProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::batteryProfile()));

    loadProfile();

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

    PowerDevilSettings::setBatLowAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());

    PowerDevilSettings::setACProfile(acProfile->currentText());
    PowerDevilSettings::setLowProfile(lowProfile->currentText());
    PowerDevilSettings::setWarningProfile(warningProfile->currentText());
    PowerDevilSettings::setBatteryProfile(batteryProfile->currentText());

    PowerDevilSettings::self()->writeConfig();

    saveProfile();
}

void ConfigWidget::emitChanged()
{
    emit changed(true);
}

void ConfigWidget::enableBoxes()
{
    displayIdleTime->setEnabled(offDisplayWhenIdle->isChecked());
}

void ConfigWidget::loadProfile()
{
    kDebug() << "Loading a profile";

    if (profileEditBox->currentText().isEmpty())
        return;

    kDebug() << profileEditBox->currentText();

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profileEditBox->currentText());

    if (!group->isValid())
        return;

    kDebug() << "Ok, KConfigGroup ready";

    kDebug() << group->readEntry("brightness");

    brightnessSlider->setValue(group->readEntry("brightness").toInt());
    offDisplayWhenIdle->setChecked(group->readEntry("turnOffIdle", false));
    displayIdleTime->setValue(group->readEntry("turnOffIdleTime").toInt());
    idleTime->setValue(group->readEntry("idleTime").toInt());
    idleCombo->setCurrentIndex(idleCombo->findData(group->readEntry("idleAction").toInt()));
    freqCombo->setCurrentIndex(freqCombo->findData(group->readEntry("cpuPolicy").toInt()));

    laptopClosedCombo->setCurrentIndex(laptopClosedCombo->findData(group->readEntry("lidAction").toInt()));

    delete group;
}

void ConfigWidget::saveProfile()
{
    if (profileEditBox->currentText().isEmpty())
        return;

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profileEditBox->currentText());

    if (!group->isValid() || !group->entryMap().size())
        return;

    group->writeEntry("brightness", brightnessSlider->value());
    group->writeEntry("cpuPolicy", freqCombo->itemData(freqCombo->currentIndex()).toInt());
    group->writeEntry("idleAction", idleCombo->itemData(idleCombo->currentIndex()).toInt());
    group->writeEntry("idleTime", idleTime->value());
    group->writeEntry("lidAction", laptopClosedCombo->itemData(laptopClosedCombo->currentIndex()).toInt());
    group->writeEntry("turnOffIdle", offDisplayWhenIdle->isChecked());
    group->writeEntry("turnOffIdleTime", displayIdleTime->value());

    group->sync();

    delete group;
}

void ConfigWidget::reloadAvailableProfiles()
{
    profileEditBox->clear();
    acProfile->clear();
    batteryProfile->clear();
    lowProfile->clear();
    warningProfile->clear();

    if (m_profilesConfig->groupList().isEmpty())
    {
        kDebug() << "No available profiles!";
        return;
    }

    profileEditBox->addItems(m_profilesConfig->groupList());
    acProfile->addItems(m_profilesConfig->groupList());
    batteryProfile->addItems(m_profilesConfig->groupList());
    lowProfile->addItems(m_profilesConfig->groupList());
    warningProfile->addItems(m_profilesConfig->groupList());
}

void ConfigWidget::deleteCurrentProfile()
{
    m_profilesConfig->deleteGroup(profileEditBox->currentText());

    m_profilesConfig->sync();

    delete m_profilesConfig;

    m_profilesConfig = new KConfig( "powerdevilprofilesrc", KConfig::SimpleConfig );

    reloadAvailableProfiles();
}

void ConfigWidget::createProfile(const QString &name)
{
    KConfigGroup *group = new KConfigGroup(m_profilesConfig, name);

    group->writeEntry("brightness", 80);
    group->writeEntry("cpuPolicy", (int) Solid::Control::PowerManager::Powersave);
    group->writeEntry("idleAction", 0);
    group->writeEntry("idleTime", 50);
    group->writeEntry("lidAction", 0);
    group->writeEntry("turnOffIdle", false);
    group->writeEntry("turnOffIdleTime", 50);

    group->sync();

    delete group;

    reloadAvailableProfiles();
}

void ConfigWidget::createProfile()
{
    KDialog *dialog = new KDialog(this);
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit();
    QLabel *lb = new QLabel();
    QVBoxLayout *lay = new QVBoxLayout();

    lb->setText(i18n("Please enter a name for the new profile"));

    lay->addWidget(lb);
    lay->addWidget(ed);

    wg->setLayout(lay);

    dialog->setMainWidget(wg);

    if ( dialog->exec() == KDialog::Accepted )
    {
        createProfile(ed->text());
    }
}

#include "ConfigWidget.moc"
