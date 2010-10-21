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
#include <KDebug>
#include <QCheckBox>
#include <QFormLayout>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KIconButton>
#include <KToolBar>
#include <KAboutData>
#include <KPluginFactory>
#include <QtGui/QLabel>
#include <KServiceTypeTrader>
#include <daemon/powerdevilactionconfig.h>
#include <QtGui/QGroupBox>
#include "actionconfigwidget.h"

K_PLUGIN_FACTORY(PowerDevilProfilesKCMFactory,
                 registerPlugin<EditPage>();
                )
K_EXPORT_PLUGIN(PowerDevilProfilesKCMFactory("kcmpowerdevilprofiles"))

EditPage::EditPage(QWidget *parent, const QVariantList &args)
        : KCModule(PowerDevilProfilesKCMFactory::componentData(), parent, args)
        , m_profileEdited(false)
{
    setButtons(Apply | Help);

    KAboutData *about =
        new KAboutData("kcmpowerdevilprofiles", "powerdevilprofiles", ki18n("Power Profiles Configuration"),
                       "", ki18n("A profile configurator for KDE Power Management System"),
                       KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
                       ki18n("From this module, you can configure the Daemon, create "
                             "and edit powersaving profiles, and see your system's "
                             "capabilities."));

    about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
                     "http://drfav.wordpress.com");

    setAboutData(about);

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

    connect(profilesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            SLOT(switchProfile(QListWidgetItem*, QListWidgetItem*)));

    connect(actionDeleteProfile, SIGNAL(triggered()), SLOT(deleteCurrentProfile()));
    connect(actionNewProfile, SIGNAL(triggered()), SLOT(createProfile()));
    connect(actionImportProfiles, SIGNAL(triggered()), SLOT(importProfiles()));
    connect(actionExportProfiles, SIGNAL(triggered()), SLOT(exportProfiles()));

    reloadAvailableProfiles();
    ActionConfigWidget *actionConfigWidget = new ActionConfigWidget(0);
    QMap< int, QList<QPair<QString, QWidget*> > > widgets;

    // Load all the services
    KService::List offers = KServiceTypeTrader::self()->query("PowerDevil/Action", "(Type == 'Service')");

    foreach (const KService::Ptr &offer, offers) {
        //try to load the specified library
        KPluginFactory *factory = KPluginLoader(offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                QVariant::String).toString()).factory();

        if (!factory) {
            kError() << "KPluginFactory could not load the plugin:" << offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                       QVariant::String).toString();
            continue;
        }

        PowerDevil::ActionConfig *actionConfig = factory->create<PowerDevil::ActionConfig>();
        if (!actionConfig) {
            kError() << "KPluginFactory could not load the plugin:" << offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                       QVariant::String).toString();
            continue;
        }

        connect(actionConfig, SIGNAL(changed()), this, SLOT(changed()));

        QCheckBox *checkbox = new QCheckBox(offer->name());
        if (!offer->icon().isEmpty()) {
            checkbox->setIcon(KIcon(offer->icon()));
        }
        connect(checkbox, SIGNAL(stateChanged(int)), this, SLOT(changed()));
        m_actionsHash.insert(offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString(), checkbox);
        m_actionsConfigHash.insert(offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString(), actionConfig);

        QList<QPair<QString, QWidget*> > offerWidgets = actionConfig->buildUi();
        offerWidgets.prepend(qMakePair<QString,QWidget*>(QString(), checkbox));
        widgets.insert(100 - offer->property("X-KDE-PowerDevil-Action-ConfigPriority", QVariant::Int).toInt(),
                       offerWidgets);
    }

    for (QMap< int, QList<QPair<QString, QWidget*> > >::const_iterator i = widgets.constBegin(); i != widgets.constEnd(); ++i) {
        actionConfigWidget->addWidgets(i.value());
    }

    // Add a proxy widget to prevent vertical fuck ups
    QWidget *tw = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout;
    lay->addWidget(actionConfigWidget);
    lay->addStretch();
    tw->setLayout(lay);
    scrollArea->setWidget(tw);
}

EditPage::~EditPage()
{
}

void EditPage::load()
{
    loadProfile();
}

void EditPage::save()
{
    saveProfile();
}

void EditPage::loadProfile()
{
    kDebug() << "Loading a profile";

    if (!profilesList->currentItem())
        return;

    kDebug() << profilesList->currentItem()->text();

    KConfigGroup group(m_profilesConfig, profilesList->currentItem()->text());

    if (!group.isValid()) {
        return;
    }
    kDebug() << "Ok, KConfigGroup ready";

    // Iterate over the possible actions
    for (QHash< QString, QCheckBox* >::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        i.value()->setChecked(group.groupList().contains(i.key()));

        KConfigGroup actionGroup = group.group(i.key());
        m_actionsConfigHash[i.key()]->setConfigGroup(actionGroup);
        m_actionsConfigHash[i.key()]->load();
    }

    emit changed(false);
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

    KConfigGroup group(m_profilesConfig, profile);

    if (!group.isValid() || !group.entryMap().size()) {
        return;
    }

    // Iterate over the possible actions
    for (QHash< QString, QCheckBox* >::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        if (i.value()->isChecked()) {
            // Perform the actual save
            m_actionsConfigHash[i.key()]->save();
        } else {
            // Erase the group
            group.group(i.key()).deleteGroup();
        }
    }

    emit changed(false);
}

void EditPage::reloadAvailableProfiles()
{
    profilesList->clear();

    m_profilesConfig->reparseConfiguration();

    if (m_profilesConfig->groupList().isEmpty()) {
        kDebug() << "No available profiles!";
        return;
    }

    foreach (const QString &ent, m_profilesConfig->groupList()) {
        KConfigGroup *group = new KConfigGroup(m_profilesConfig, ent);
        QListWidgetItem *itm = new QListWidgetItem(KIcon(group->readEntry("iconname")), ent);
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
}

void EditPage::createProfile(const QString &name, const QString &icon)
{
    if (name.isEmpty()) {
        return;
    }
    KConfigGroup group(m_profilesConfig, name);

    group.sync();

    reloadAvailableProfiles();
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

    KConfigGroup group(m_profilesConfig, prevname);

    group.writeEntry("iconname", icon);

    group.sync();

    reloadAvailableProfiles();


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

void EditPage::openUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}

void EditPage::defaults()
{
    KCModule::defaults();
}

#include "EditPage.moc"
