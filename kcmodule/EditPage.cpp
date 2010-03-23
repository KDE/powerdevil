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

#include "EditPage.h"

#include "PowerDevilSettings.h"

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KStandardDirs>
#include <KRun>

#include <config-workspace.h>

#include <KConfigGroup>
#include <KLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KIconButton>
#include <KToolBar>

EditPage::EditPage(QWidget *parent)
        : QWidget(parent),
        m_profileEdited(false)
{
    setupUi(this);

    m_profilesConfig = KSharedConfig::openConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
        // Let's add some basic profiles, huh?

        KConfigGroup *performance = new KConfigGroup(m_profilesConfig, "Performance");

        performance->writeEntry("brightness", 100);
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

EditPage::~EditPage()
{
}

void EditPage::fillUi()
{
    m_toolBar = new KToolBar(this);
    listLayout->addWidget(m_toolBar);

    m_toolBar->addAction(actionNewProfile);
    m_toolBar->addAction(actionDeleteProfile);
    m_toolBar->addSeparator();
    m_toolBar->addAction(actionImportProfiles);
    m_toolBar->addAction(actionExportProfiles);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    actionNewProfile->setIcon(KIcon("document-new"));
    actionDeleteProfile->setIcon(KIcon("edit-delete-page"));
    actionImportProfiles->setIcon(KIcon("document-import"));
    actionExportProfiles->setIcon(KIcon("document-export"));

    idleCombo->addItem(KIcon("dialog-cancel"), i18n("Do Nothing"), (int) None);
    idleCombo->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (int) Shutdown);
    idleCombo->addItem(KIcon("system-lock-screen"), i18n("Lock Screen"), (int) Lock);
    idleCombo->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn Off Screen"), (int) TurnOffScreen);
    laptopClosedCombo->addItem(KIcon("dialog-cancel"), i18n("Do Nothing"), (int) None);
    laptopClosedCombo->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (int) Shutdown);
    laptopClosedCombo->addItem(KIcon("system-lock-screen"), i18n("Lock Screen"), (int) Lock);
    laptopClosedCombo->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn Off Screen"), (int) TurnOffScreen);
    sleepButtonCombo->addItem(KIcon("dialog-cancel"), i18n("Do Nothing"), (int) None);
    sleepButtonCombo->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (int) Shutdown);
    sleepButtonCombo->addItem(KIcon("system-lock-screen"), i18n("Lock Screen"), (int) Lock);
    sleepButtonCombo->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn Off Screen"), (int) TurnOffScreen);
    sleepButtonCombo->addItem(KIcon("system-log-out"), i18n("Prompt Log Out dialog"), (int) ShutdownDialog);
    powerButtonCombo->addItem(KIcon("dialog-cancel"), i18n("Do Nothing"), (int) None);
    powerButtonCombo->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (int) Shutdown);
    powerButtonCombo->addItem(KIcon("system-lock-screen"), i18n("Lock Screen"), (int) Lock);
    powerButtonCombo->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn Off Screen"), (int) TurnOffScreen);
    powerButtonCombo->addItem(KIcon("system-log-out"), i18n("Prompt Log Out dialog"), (int) ShutdownDialog);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    Solid::Control::PowerManager::BrightnessControlsList bControls =
        Solid::Control::PowerManager::brightnessControlsAvailable();

    brightnessSlider->setEnabled(bControls.values().contains(Solid::Control::PowerManager::Screen));

    if (methods & Solid::Control::PowerManager::ToDisk) {
        idleCombo->addItem(KIcon("system-suspend-hibernate"), i18n("Suspend to Disk"), (int) S2Disk);
        laptopClosedCombo->addItem(KIcon("system-suspend-hibernate"), i18n("Suspend to Disk"), (int) S2Disk);
        sleepButtonCombo->addItem(KIcon("system-suspend-hibernate"), i18n("Suspend to Disk"), (int) S2Disk);
        powerButtonCombo->addItem(KIcon("system-suspend-hibernate"), i18n("Suspend to Disk"), (int) S2Disk);
    }

    if (methods & Solid::Control::PowerManager::ToRam) {
        idleCombo->addItem(KIcon("system-suspend"), i18n("Suspend to RAM"), (int) S2Ram);
        laptopClosedCombo->addItem(KIcon("system-suspend"), i18n("Suspend to RAM"), (int) S2Ram);
        sleepButtonCombo->addItem(KIcon("system-suspend"), i18n("Suspend to RAM"), (int) S2Ram);
        powerButtonCombo->addItem(KIcon("system-suspend"), i18n("Suspend to RAM"), (int) S2Ram);
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        idleCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
        laptopClosedCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
        sleepButtonCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
        powerButtonCombo->addItem(KIcon("system-suspend"), i18n("Standby"), (int) Standby);
    }

    schemeCombo->addItems(Solid::Control::PowerManager::supportedSchemes());

    reloadAvailableProfiles();

    tabWidget->setTabIcon(0, KIcon("preferences-system-session-services"));
    tabWidget->setTabIcon(1, KIcon("video-display"));
    tabWidget->setTabIcon(2, KIcon("cpu"));

#if 0 // Re-enable when / if we have permission to use official logo
    DPMSLabel->setUrl("http://www.energystar.gov");
    DPMSLabel->setPixmap(QPixmap(KStandardDirs::locate("data", "kcontrol/pics/energybig.png")));
    DPMSLabel->setTipText(i18n("Learn more about the Energy Star program"));
    DPMSLabel->setUseTips(true);
    connect(DPMSLabel, SIGNAL(leftClickedUrl(const QString&)), SLOT(openUrl(const QString &)));
#endif

#ifndef HAVE_DPMS
    DPMSEnable->setEnabled(false);
    DPMSSuspend->setEnabled(false);
    DPMSStandby->setEnabled(false);
    DPMSPowerOff->setEnabled(false);
#endif

    // modified fields...

    connect(brightnessSlider, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(disableCompositing, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(dimOnIdleTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(idleTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(idleCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(laptopClosedCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(sleepButtonCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(powerButtonCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));

    connect(dimDisplayOnIdle, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));

    connect(schemeCombo, SIGNAL(currentIndexChanged(int)), SLOT(setProfileChanged()));
    connect(scriptRequester, SIGNAL(textChanged(const QString&)), SLOT(setProfileChanged()));

#ifdef HAVE_DPMS
    connect(DPMSEnable, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
    connect(DPMSEnable, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSSuspendTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSStandbyTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSPowerOffTime, SIGNAL(valueChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSSuspendEnabled, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSStandbyEnabled, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));
    connect(DPMSPowerOffEnabled, SIGNAL(stateChanged(int)), SLOT(setProfileChanged()));

    connect(DPMSSuspendEnabled, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
    connect(DPMSStandbyEnabled, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
    connect(DPMSPowerOffEnabled, SIGNAL(stateChanged(int)), SLOT(enableBoxes()));
#endif

    connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));

    connect(actionDeleteProfile, SIGNAL(triggered()), SLOT(deleteCurrentProfile()));
    connect(actionNewProfile, SIGNAL(triggered()), SLOT(createProfile()));
    //connect(editProfileButton, SIGNAL(clicked()), SLOT(editProfile()));
    connect(actionImportProfiles, SIGNAL(triggered()), SLOT(importProfiles()));
    connect(actionExportProfiles, SIGNAL(triggered()), SLOT(exportProfiles()));
}

void EditPage::load()
{
    loadProfile();

    enableBoxes();
}

void EditPage::save()
{
    saveProfile();
}

void EditPage::emitChanged()
{
    emit changed(true);
}

void EditPage::enableBoxes()
{
#ifdef HAVE_DPMS
    if (DPMSEnable->isChecked()) {
        DPMSSuspendEnabled->setEnabled(true);
        DPMSStandbyEnabled->setEnabled(true);
        DPMSPowerOffEnabled->setEnabled(true);
        DPMSSuspendTime->setEnabled(DPMSSuspendEnabled->isChecked());
        DPMSStandbyTime->setEnabled(DPMSStandbyEnabled->isChecked());
        DPMSPowerOffTime->setEnabled(DPMSPowerOffEnabled->isChecked());
    } else {
        DPMSSuspendEnabled->setEnabled(false);
        DPMSStandbyEnabled->setEnabled(false);
        DPMSPowerOffEnabled->setEnabled(false);
        DPMSSuspendTime->setEnabled(false);
        DPMSStandbyTime->setEnabled(false);
        DPMSPowerOffTime->setEnabled(false);
    }
#endif

    dimOnIdleTime->setEnabled(dimDisplayOnIdle->isChecked());
}

void EditPage::loadProfile()
{
    kDebug() << "Loading a profile";

    if (!profilesList->currentItem())
        return;

    kDebug() << profilesList->currentItem()->text();

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profilesList->currentItem()->text());

    if (!group->isValid()) {
        delete group;
        return;
    }
    kDebug() << "Ok, KConfigGroup ready";

    kDebug() << group->readEntry("brightness");

    brightnessSlider->setValue(group->readEntry("brightness").toInt());
    disableCompositing->setChecked(group->readEntry("disableCompositing", false));
    dimDisplayOnIdle->setChecked(group->readEntry("dimOnIdle", false));
    dimOnIdleTime->setValue(group->readEntry("dimOnIdleTime").toInt());
    idleTime->setValue(group->readEntry("idleTime").toInt());
    idleCombo->setCurrentIndex(idleCombo->findData(group->readEntry("idleAction").toInt()));
    schemeCombo->setCurrentIndex(schemeCombo->findText(group->readEntry("scheme")));
    scriptRequester->setUrl(KUrl::fromPath(group->readEntry("scriptpath")));

    laptopClosedCombo->setCurrentIndex(laptopClosedCombo->findData(group->readEntry("lidAction").toInt()));
    sleepButtonCombo->setCurrentIndex(sleepButtonCombo->findData(group->readEntry("sleepButtonAction").toInt()));
    powerButtonCombo->setCurrentIndex(powerButtonCombo->findData(group->readEntry("powerButtonAction").toInt()));

#ifdef HAVE_DPMS
    DPMSEnable->setChecked(group->readEntry("DPMSEnabled", false));
    DPMSStandbyTime->setValue(group->readEntry("DPMSStandby", 10));
    DPMSSuspendTime->setValue(group->readEntry("DPMSSuspend", 30));
    DPMSPowerOffTime->setValue(group->readEntry("DPMSPowerOff", 60));
    DPMSStandbyEnabled->setChecked(group->readEntry("DPMSStandbyEnabled", false));
    DPMSSuspendEnabled->setChecked(group->readEntry("DPMSSuspendEnabled", false));
    DPMSPowerOffEnabled->setChecked(group->readEntry("DPMSPowerOffEnabled", false));
#endif

    delete group;

    m_profileEdited = false;
    enableSaveProfile();
}

void EditPage::saveProfile(const QString &p)
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
        delete group;
        return;
    }

    group->writeEntry("brightness", brightnessSlider->value());
    group->writeEntry("dimOnIdle", dimDisplayOnIdle->isChecked());
    group->writeEntry("dimOnIdleTime", dimOnIdleTime->value());
    group->writeEntry("idleAction", idleCombo->itemData(idleCombo->currentIndex()).toInt());
    group->writeEntry("idleTime", idleTime->value());
    group->writeEntry("lidAction", laptopClosedCombo->itemData(laptopClosedCombo->currentIndex()).toInt());
    group->writeEntry("sleepButtonAction", sleepButtonCombo->itemData(sleepButtonCombo->currentIndex()).toInt());
    group->writeEntry("powerButtonAction", powerButtonCombo->itemData(powerButtonCombo->currentIndex()).toInt());
    group->writeEntry("scheme", schemeCombo->currentText());
    group->writeEntry("scriptpath", scriptRequester->url().path());
    group->writeEntry("disableCompositing", disableCompositing->isChecked());

#ifdef HAVE_DPMS
    group->writeEntry("DPMSEnabled", DPMSEnable->isChecked());
    group->writeEntry("DPMSStandby", DPMSStandbyTime->value());
    group->writeEntry("DPMSSuspend", DPMSSuspendTime->value());
    group->writeEntry("DPMSPowerOff", DPMSPowerOffTime->value());
    group->writeEntry("DPMSStandbyEnabled", DPMSStandbyEnabled->isChecked());
    group->writeEntry("DPMSSuspendEnabled", DPMSSuspendEnabled->isChecked());
    group->writeEntry("DPMSPowerOffEnabled", DPMSPowerOffEnabled->isChecked());
#endif

    group->sync();

    delete group;

    m_profileEdited = false;
    enableSaveProfile();

    emit profilesChanged();
}

void EditPage::reloadAvailableProfiles()
{
    profilesList->clear();

    m_profilesConfig->reparseConfiguration();

    if (m_profilesConfig->groupList().isEmpty()) {
        kDebug() << "No available profiles!";
        return;
    }

    foreach(const QString &ent, m_profilesConfig->groupList()) {
        KConfigGroup *group = new KConfigGroup(m_profilesConfig, ent);
        QListWidgetItem *itm = new QListWidgetItem(KIcon(group->readEntry("iconname")),
                ent);
        profilesList->addItem(itm);
        delete group;
    }

    profilesList->setCurrentRow(0);
}

void EditPage::deleteCurrentProfile()
{
    if (!profilesList->currentItem() || profilesList->currentItem()->text().isEmpty()) {
        return;
    }

    // We're deleting it, we don't care anymore
    m_profileEdited = false;

    m_profilesConfig->deleteGroup(profilesList->currentItem()->text());
    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::createProfile(const QString &name, const QString &icon)
{
    if (name.isEmpty())
        return;
    KConfigGroup *group = new KConfigGroup(m_profilesConfig, name);

    group->writeEntry("brightness", 80);
    group->writeEntry("idleAction", 0);
    group->writeEntry("idleTime", 50);
    group->writeEntry("lidAction", 0);
    group->writeEntry("turnOffIdle", false);
    group->writeEntry("turnOffIdleTime", 50);
    group->writeEntry("iconname", icon);

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::createProfile()
{
    KDialog *dialog = new KDialog(this);
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit(wg);
    QLabel *lb = new QLabel(wg);
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton(wg);

    ibt->setIconSize(KIconLoader::SizeSmall);

    lb->setText(i18n("Please enter a name for the new profile:"));

    lb->setToolTip(i18n("The name for the new profile"));
    lb->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    ed->setToolTip(i18n("The name for the new profile"));
    ed->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    lay->addRow(lb);
    lay->addRow(ibt, ed);

    wg->setLayout(lay);

    dialog->setMainWidget(wg);
    ed->setFocus();

    if (dialog->exec() == KDialog::Accepted) {
        createProfile(ed->text(), ibt->icon());
    }
    delete dialog;
}

void EditPage::editProfile(const QString &prevname, const QString &icon)
{
    if (prevname.isEmpty())
        return;

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, prevname);

    group->writeEntry("iconname", icon);

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::editProfile()
{
    if (!profilesList->currentItem())
        return;

    KDialog *dialog = new KDialog(this);
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit(wg);
    QLabel *lb = new QLabel(wg);
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton(wg);

    ibt->setIconSize(KIconLoader::SizeSmall);

    lb->setText(i18n("Please enter a name for this profile:"));

    lb->setToolTip(i18n("The name for the new profile"));
    lb->setWhatsThis(i18n("Enter here the name for the profile you are creating"));

    ed->setToolTip(i18n("The name for the new profile"));
    ed->setWhatsThis(i18n("Enter here the name for the profile you are creating"));
    ed->setEnabled(false);

    ed->setText(profilesList->currentItem()->text());

    KConfigGroup *group = new KConfigGroup(m_profilesConfig, profilesList->currentItem()->text());

    ibt->setIcon(group->readEntry("iconname"));

    lay->addRow(lb);
    lay->addRow(ibt, ed);

    wg->setLayout(lay);

    dialog->setMainWidget(wg);
    ed->setFocus();

    if (dialog->exec() == KDialog::Accepted) {
        editProfile(profilesList->currentItem()->text(), ibt->icon());
    }

    delete dialog;
    delete group;
}

void EditPage::importProfiles()
{
    QString fileName = KFileDialog::getOpenFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Import PowerDevil Profiles"));

    if (fileName.isEmpty()) {
        return;
    }

    KConfig toImport(fileName, KConfig::SimpleConfig);

    foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup copyFrom(&toImport, ent);
        KConfigGroup copyTo(m_profilesConfig, ent);

        copyFrom.copyTo(&copyTo);
    }

    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void EditPage::exportProfiles()
{
    QString fileName = KFileDialog::getSaveFileName(KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n("Export PowerDevil Profiles"));

    if (fileName.isEmpty()) {
        return;
    }

    kDebug() << "Filename is" << fileName;

    KConfig *toExport = m_profilesConfig->copyTo(fileName);

    toExport->sync();

    delete toExport;
}

void EditPage::switchProfile(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(current)

    if (!m_profileEdited) {
        loadProfile();
    } else {
        if (!previous) {
             // Pass by, the profile has probably been deleted
            loadProfile();
            return;
        } else if (!m_profilesConfig.data()->groupList().contains(previous->text())) {
            // Pass by, the profile has probably been deleted
            loadProfile();
            return;
        }

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

void EditPage::setProfileChanged()
{
    m_profileEdited = true;
    emitChanged();
}

void EditPage::enableSaveProfile()
{
}

void EditPage::openUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}

#include "EditPage.moc"
