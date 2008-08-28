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

#include "ConfigWidget.h"

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <KConfigGroup>
#include <KLineEdit>
#include <QCheckBox>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>

ConfigWidget::ConfigWidget(QWidget *parent)
        : QWidget(parent),
        m_profileEdited(false)
{
    setupUi(this);

    m_profilesConfig = new KConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
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

    schemeCombo->addItems(Solid::Control::PowerManager::supportedSchemes());


    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*>(d.asDeviceInterface(Solid::DeviceInterface::Processor));

        QString text = QString("CPU %1").arg(processor->number());

        QCheckBox *checkBox = new QCheckBox(this);

        checkBox->setText(text);

        checkBox->setEnabled(Solid::Control::PowerManager::canDisableCpu(processor->number()));

        connect(checkBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

        CPUListLayout->addWidget(checkBox);
    }

    reloadAvailableProfiles();

    newProfile->setIcon(KIcon("document-new"));
    deleteProfile->setIcon(KIcon("edit-delete-page"));
    importButton->setIcon(KIcon("document-import"));
    exportButton->setIcon(KIcon("document-export"));
    saveCurrentProfileButton->setIcon(KIcon("document-save"));
    resetCurrentProfileButton->setIcon(KIcon("edit-undo"));

    fillCapabilities();

    // modified fields...

    connect(lockScreenOnResume, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(dimOnIdleTime, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(notificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));
    connect(warningNotificationsBox, SIGNAL(stateChanged(int)), SLOT(emitChanged()));

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(warningSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(emitChanged()));

    connect(brightnessSlider, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(offDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(displayIdleTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(idleTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(idleCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(freqCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(laptopClosedCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(offDisplayWhenIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(schemeCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));

    connect(acProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(lowProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(warningProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));
    connect(batteryProfile, SIGNAL(currentIndexChanged(int)), SLOT(emitChanged()));

    connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));

    connect(deleteProfile, SIGNAL(clicked()), SLOT(deleteCurrentProfile()));
    connect(newProfile, SIGNAL(clicked()), SLOT(createProfile()));
    connect(importButton, SIGNAL(clicked()), SLOT(importProfiles()));
    connect(exportButton, SIGNAL(clicked()), SLOT(exportProfiles()));
    connect(resetCurrentProfileButton, SIGNAL(clicked()), SLOT(loadProfile()));
    connect(saveCurrentProfileButton, SIGNAL(clicked()), SLOT(saveProfile()));
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

    if (!profilesList->currentItem())
        return;

    kDebug() << profilesList->currentItem()->text();

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profilesList->currentItem()->text());

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
    schemeCombo->setCurrentIndex(schemeCombo->findText(group->readEntry("scheme")));

    laptopClosedCombo->setCurrentIndex(laptopClosedCombo->findData(group->readEntry("lidAction").toInt()));

    QVariant var = group->readEntry("disabledCPUs", QVariant());
    QList<QVariant> list = var.toList();

    foreach(const QVariant &ent, list) {
        QCheckBox *box = qobject_cast<QCheckBox*>(CPUListLayout->itemAt(ent.toInt())->widget());

        if (!box)
            continue;

        box->setChecked(true);
    }

    delete group;

    m_profileEdited = false;
    enableSaveProfile();
}

void ConfigWidget::saveProfile(const QString &p)
{
    if (!profilesList->currentItem() && p.isEmpty()) {
        return;
    }

    QString profile;

    if (p.isEmpty()) {
        profile = profilesList->currentItem()->text();
    } else {
        profile = p;
    }

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profile);

    if (!group->isValid() || !group->entryMap().size()) {
        return;
    }

    group->writeEntry("brightness", brightnessSlider->value());
    group->writeEntry("cpuPolicy", freqCombo->itemData(freqCombo->currentIndex()).toInt());
    group->writeEntry("idleAction", idleCombo->itemData(idleCombo->currentIndex()).toInt());
    group->writeEntry("idleTime", idleTime->value());
    group->writeEntry("lidAction", laptopClosedCombo->itemData(laptopClosedCombo->currentIndex()).toInt());
    group->writeEntry("turnOffIdle", offDisplayWhenIdle->isChecked());
    group->writeEntry("turnOffIdleTime", displayIdleTime->value());
    group->writeEntry("scheme", schemeCombo->currentText());

    QList<int> list;

    for (int i = 0;i < CPUListLayout->count();i++) {
        QCheckBox *box = qobject_cast<QCheckBox*>(CPUListLayout->itemAt(i)->widget());

        if (!box)
            continue;

        if (box->isChecked())
            list.append(i);
    }

    group->writeEntry("disabledCPUs", list);

    group->sync();

    delete group;

    m_profileEdited = false;
    enableSaveProfile();

    emit profilesChanged();
}

void ConfigWidget::reloadAvailableProfiles()
{
    profilesList->clear();
    acProfile->clear();
    batteryProfile->clear();
    lowProfile->clear();
    warningProfile->clear();

    if (m_profilesConfig->groupList().isEmpty()) {
        kDebug() << "No available profiles!";
        return;
    }

    profilesList->addItems(m_profilesConfig->groupList());
    acProfile->addItems(m_profilesConfig->groupList());
    batteryProfile->addItems(m_profilesConfig->groupList());
    lowProfile->addItems(m_profilesConfig->groupList());
    warningProfile->addItems(m_profilesConfig->groupList());

    acProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::aCProfile()));
    lowProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::lowProfile()));
    warningProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::warningProfile()));
    batteryProfile->setCurrentIndex(acProfile->findText(PowerDevilSettings::batteryProfile()));

    profilesList->setCurrentRow(0);
}

void ConfigWidget::deleteCurrentProfile()
{
    m_profilesConfig->deleteGroup(profilesList->currentItem()->text());

    m_profilesConfig->sync();

    delete m_profilesConfig;

    m_profilesConfig = new KConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    reloadAvailableProfiles();

    emit profilesChanged();
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

    emit profilesChanged();
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

    if (dialog->exec() == KDialog::Accepted) {
        createProfile(ed->text());
    }
}

void ConfigWidget::fillCapabilities()
{
    int batteryCount = 0;
    int cpuCount = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Q_UNUSED(device)
        batteryCount++;
    }

    bool freqchange = false;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*>(d.asDeviceInterface(Solid::DeviceInterface::Processor));

        if (processor->canChangeFrequency()) {
            freqchange = true;
        }

        cpuCount++;
    }

    if (freqchange)
        isScalingSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    else
        isScalingSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));

    cpuNumber->setText(QString("%1").arg(cpuCount));
    batteriesNumber->setText(QString("%1").arg(batteryCount));

    bool turnOff = false;

    for (int i = 0;i < cpuCount;i++) {
        if (Solid::Control::PowerManager::canDisableCpu(i))
            turnOff = true;
    }

    if (turnOff)
        isCPUOffSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    else
        isCPUOffSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));

    QString sMethods;

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods | Solid::Control::PowerManager::ToDisk) {
        sMethods.append(QString(i18n("Suspend to Disk") + QString(", ")));
    }

    if (methods | Solid::Control::PowerManager::ToRam) {
        sMethods.append(QString(i18n("Suspend to RAM") + QString(", ")));
    }

    if (methods | Solid::Control::PowerManager::Standby) {
        sMethods.append(QString(i18n("Standby") + QString(", ")));
    }

    sMethods.remove(sMethods.length() - 2, 2);

    supportedMethods->setText(sMethods);

    QString scMethods;

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies | Solid::Control::PowerManager::Performance) {
        scMethods.append(QString(i18n("Performance") + QString(", ")));
    }

    if (policies | Solid::Control::PowerManager::OnDemand) {
        scMethods.append(QString(i18n("Dynamic (ondemand)") + QString(", ")));
    }

    if (policies | Solid::Control::PowerManager::Conservative) {
        scMethods.append(QString(i18n("Dynamic (conservative)") + QString(", ")));
    }

    if (policies | Solid::Control::PowerManager::Powersave) {
        scMethods.append(QString(i18n("Powersave") + QString(", ")));
    }

    scMethods.remove(scMethods.length() - 2, 2);

    supportedPolicies->setText(scMethods);

    if (!Solid::Control::PowerManager::supportedSchemes().isEmpty())
        isSchemeSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    else
        isSchemeSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));

    QString schemes;

    foreach(const QString &scheme, Solid::Control::PowerManager::supportedSchemes()) {
        schemes.append(scheme + QString(", "));
    }

    schemes.remove(schemes.length() - 2, 2);

    supportedSchemes->setText(schemes);
}

void ConfigWidget::importProfiles()
{
    QString fileName = KFileDialog::getOpenFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Import PowerDevil profiles"));

    KConfig toImport(fileName, KConfig::SimpleConfig);

    // FIXME: This should be the correct way, but why it doesn't work?
    /*foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup group = toImport.group(ent);

        group.copyTo(m_profilesConfig);
    }*/

    // FIXME: This way works, but the import file gets cleared, definitely not what we want
    foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup group = toImport.group(ent);
        KConfigGroup group2(group);

        group2.reparent(m_profilesConfig);
    }

    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void ConfigWidget::exportProfiles()
{
    QString fileName = KFileDialog::getSaveFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Export PowerDevil profiles"));

    kDebug() << "Filename is" << fileName;

    KConfig *toExport = m_profilesConfig->copyTo(fileName);

    toExport->sync();

    delete toExport;
}

void ConfigWidget::switchProfile(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(current)

    if (!m_profileEdited) {
        loadProfile();
    } else {
        int result = KMessageBox::warningYesNoCancel(this, i18n("The current profile has not been saved.\n"
                     "Do you want to save it?"), i18n("Save Profile"));

        if (result == KMessageBox::Yes) {
            saveProfile(previous->text());
            loadProfile();
        } else if (result == KMessageBox::No) {
            loadProfile();
        } else if (result == KMessageBox::Cancel) {
            disconnect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
                       this, SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));
            profilesList->setCurrentItem(previous);
            connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
                    SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));
        }
    }
}

void ConfigWidget::setProfileChanged()
{
    m_profileEdited = true;
    enableSaveProfile();
}

void ConfigWidget::enableSaveProfile()
{
    saveCurrentProfileButton->setEnabled(m_profileEdited);
}

#include "ConfigWidget.moc"
