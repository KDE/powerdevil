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

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>

#include <KConfigGroup>
#include <KLineEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <KDialog>
#include <KFileDialog>
#include <KMessageBox>
#include <KIconButton>

ConfigWidget::ConfigWidget( QWidget *parent )
        : QWidget( parent ),
        m_profileEdited( false )
{
    setupUi( this );

    m_profilesConfig = new KConfig( "powerdevilprofilesrc", KConfig::SimpleConfig );

    if ( m_profilesConfig->groupList().isEmpty() ) {
        // Let's add some basic profiles, huh?

        KConfigGroup *performance = new KConfigGroup( m_profilesConfig, "Performance" );

        performance->writeEntry( "brightness", 100 );
        performance->writeEntry( "cpuPolicy", ( int ) Solid::Control::PowerManager::Performance );
        performance->writeEntry( "idleAction", 0 );
        performance->writeEntry( "idleTime", 50 );
        performance->writeEntry( "lidAction", 0 );
        performance->writeEntry( "turnOffIdle", false );
        performance->writeEntry( "turnOffIdleTime", 120 );

        performance->sync();

        kDebug() << performance->readEntry( "brightness" );

        delete performance;
    }

    fillUi();
}

ConfigWidget::~ConfigWidget()
{
    delete m_profilesConfig;
}

void ConfigWidget::fillUi()
{
    idleCombo->addItem( i18n( "Do nothing" ), ( int ) None );
    idleCombo->addItem( i18n( "Shutdown" ), ( int ) Shutdown );
    idleCombo->addItem( i18n( "Lock Screen" ), ( int ) Lock );
    BatteryCriticalCombo->addItem( i18n( "Do nothing" ), ( int ) None );
    BatteryCriticalCombo->addItem( i18n( "Shutdown" ), ( int ) Shutdown );
    laptopClosedCombo->addItem( i18n( "Do nothing" ), ( int ) None );
    laptopClosedCombo->addItem( i18n( "Shutdown" ), ( int ) Shutdown );
    laptopClosedCombo->addItem( i18n( "Lock Screen" ), ( int ) Lock );

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if ( methods & Solid::Control::PowerManager::ToDisk ) {
        idleCombo->addItem( i18n( "Suspend to Disk" ), ( int ) S2Disk );
        BatteryCriticalCombo->addItem( i18n( "Suspend to Disk" ), ( int ) S2Disk );
        laptopClosedCombo->addItem( i18n( "Suspend to Disk" ), ( int ) S2Disk );
    }

    if ( methods & Solid::Control::PowerManager::ToRam ) {
        idleCombo->addItem( i18n( "Suspend to Ram" ), ( int ) S2Ram );
        BatteryCriticalCombo->addItem( i18n( "Suspend to Ram" ), ( int ) S2Ram );
        laptopClosedCombo->addItem( i18n( "Suspend to Ram" ), ( int ) S2Ram );
    }

    if ( methods & Solid::Control::PowerManager::Standby ) {
        idleCombo->addItem( i18n( "Standby" ), ( int ) Standby );
        BatteryCriticalCombo->addItem( i18n( "Standby" ), ( int ) Standby );
        laptopClosedCombo->addItem( i18n( "Standby" ), ( int ) Standby );
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if ( policies & Solid::Control::PowerManager::Performance ) {
        freqCombo->addItem( i18n( "Performance" ), ( int ) Solid::Control::PowerManager::Performance );
    }

    if ( policies & Solid::Control::PowerManager::OnDemand ) {
        freqCombo->addItem( i18n( "Dynamic (ondemand)" ), ( int ) Solid::Control::PowerManager::OnDemand );
    }

    if ( policies & Solid::Control::PowerManager::Conservative ) {
        freqCombo->addItem( i18n( "Dynamic (conservative)" ), ( int ) Solid::Control::PowerManager::Conservative );
    }

    if ( policies & Solid::Control::PowerManager::Powersave ) {
        freqCombo->addItem( i18n( "Powersave" ), ( int ) Solid::Control::PowerManager::Powersave );
    }

    if ( policies & Solid::Control::PowerManager::Userspace ) {
        freqCombo->addItem( i18n( "Userspace" ), ( int ) Solid::Control::PowerManager::Userspace );
    }

    schemeCombo->addItems( Solid::Control::PowerManager::supportedSchemes() );


    foreach( const Solid::Device &device, Solid::Device::listFromType( Solid::DeviceInterface::Processor, QString() ) ) {
        Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*> ( d.asDeviceInterface( Solid::DeviceInterface::Processor ) );

        QString text = i18n( "CPU <numid>%1</numid>", processor->number() );

        QCheckBox *checkBox = new QCheckBox( this );

        checkBox->setText( text );
        checkBox->setToolTip( i18n( "Disable CPU <numid>%1</numid>", processor->number() ) );
        checkBox->setWhatsThis( i18n( "If this box is checked, the CPU <numid>%1</numid> "
                                      "will be disabled", processor->number() ) );

        checkBox->setEnabled( Solid::Control::PowerManager::canDisableCpu( processor->number() ) );

        connect( checkBox, SIGNAL( stateChanged( int ) ), SLOT( emitChanged() ) );

        CPUListLayout->addWidget( checkBox );
    }

    reloadAvailableProfiles();

    newProfile->setIcon( KIcon( "document-new" ) );
    editProfileButton->setIcon( KIcon( "edit-rename" ) );
    deleteProfile->setIcon( KIcon( "edit-delete-page" ) );
    importButton->setIcon( KIcon( "document-import" ) );
    exportButton->setIcon( KIcon( "document-export" ) );
    saveCurrentProfileButton->setIcon( KIcon( "document-save" ) );
    resetCurrentProfileButton->setIcon( KIcon( "edit-undo" ) );

    QDBusMessage msg = QDBusMessage::createMethodCall( "org.kde.kded",
                       "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedPollingSystems" );
    QDBusReply<QVariantMap> systems = QDBusConnection::sessionBus().call( msg );

    foreach( const QString &ent, systems.value().keys() ) {
        pollingSystemBox->addItem( ent, systems.value()[ent].toInt() );
    }

    fillCapabilities();

    // modified fields...

    connect( lockScreenOnResume, SIGNAL( stateChanged( int ) ), SLOT( emitChanged() ) );
    connect( dimDisplayOnIdle, SIGNAL( stateChanged( int ) ), SLOT( emitChanged() ) );
    connect( dimOnIdleTime, SIGNAL( valueChanged( int ) ), SLOT( emitChanged() ) );
    connect( notificationsBox, SIGNAL( stateChanged( int ) ), SLOT( emitChanged() ) );
    connect( warningNotificationsBox, SIGNAL( stateChanged( int ) ), SLOT( emitChanged() ) );
    connect( pollingSystemBox, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );

    connect( lowSpin, SIGNAL( valueChanged( int ) ), SLOT( emitChanged() ) );
    connect( warningSpin, SIGNAL( valueChanged( int ) ), SLOT( emitChanged() ) );
    connect( criticalSpin, SIGNAL( valueChanged( int ) ), SLOT( emitChanged() ) );

    connect( brightnessSlider, SIGNAL( valueChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( offDisplayWhenIdle, SIGNAL( stateChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( displayIdleTime, SIGNAL( valueChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( idleTime, SIGNAL( valueChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( idleCombo, SIGNAL( currentIndexChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( freqCombo, SIGNAL( currentIndexChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( laptopClosedCombo, SIGNAL( currentIndexChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( offDisplayWhenIdle, SIGNAL( stateChanged( int ) ), SLOT( enableBoxes() ) );

    connect( BatteryCriticalCombo, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );
    connect( schemeCombo, SIGNAL( currentIndexChanged( int ) ), SLOT( setProfileChanged() ) );
    connect( scriptRequester, SIGNAL( textChanged( const QString& ) ), SLOT( setProfileChanged() ) );

    connect( acProfile, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );
    connect( lowProfile, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );
    connect( warningProfile, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );
    connect( batteryProfile, SIGNAL( currentIndexChanged( int ) ), SLOT( emitChanged() ) );

    connect( profilesList, SIGNAL( currentItemChanged( QListWidgetItem*, QListWidgetItem* ) ),
             SLOT( switchProfile( QListWidgetItem*, QListWidgetItem* ) ) );

    connect( deleteProfile, SIGNAL( clicked() ), SLOT( deleteCurrentProfile() ) );
    connect( newProfile, SIGNAL( clicked() ), SLOT( createProfile() ) );
    connect( editProfileButton, SIGNAL( clicked() ), SLOT( editProfile() ) );
    connect( importButton, SIGNAL( clicked() ), SLOT( importProfiles() ) );
    connect( exportButton, SIGNAL( clicked() ), SLOT( exportProfiles() ) );
    connect( resetCurrentProfileButton, SIGNAL( clicked() ), SLOT( loadProfile() ) );
    connect( saveCurrentProfileButton, SIGNAL( clicked() ), SLOT( saveProfile() ) );
}

void ConfigWidget::load()
{
    lockScreenOnResume->setChecked( PowerDevilSettings::configLockScreen() );
    dimDisplayOnIdle->setChecked( PowerDevilSettings::dimOnIdle() );
    dimOnIdleTime->setValue( PowerDevilSettings::dimOnIdleTime() );
    notificationsBox->setChecked( PowerDevilSettings::enableNotifications() );
    warningNotificationsBox->setChecked( PowerDevilSettings::enableWarningNotifications() );
    pollingSystemBox->setCurrentIndex( pollingSystemBox->findData( PowerDevilSettings::pollingSystem() ) );

    lowSpin->setValue( PowerDevilSettings::batteryLowLevel() );
    warningSpin->setValue( PowerDevilSettings::batteryWarningLevel() );
    criticalSpin->setValue( PowerDevilSettings::batteryCriticalLevel() );

    BatteryCriticalCombo->setCurrentIndex( BatteryCriticalCombo->findData( PowerDevilSettings::batLowAction() ) );

    acProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::aCProfile() ) );
    lowProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::lowProfile() ) );
    warningProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::warningProfile() ) );
    batteryProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::batteryProfile() ) );

    loadProfile();

    enableBoxes();
}

void ConfigWidget::save()
{
    PowerDevilSettings::setConfigLockScreen( lockScreenOnResume->isChecked() );
    PowerDevilSettings::setDimOnIdle( dimDisplayOnIdle->isChecked() );
    PowerDevilSettings::setDimOnIdleTime( dimOnIdleTime->value() );
    PowerDevilSettings::setEnableNotifications( notificationsBox->isChecked() );
    PowerDevilSettings::setEnableWarningNotifications( warningNotificationsBox->isChecked() );
    PowerDevilSettings::setPollingSystem( pollingSystemBox->itemData( pollingSystemBox->currentIndex() ).toInt() );

    PowerDevilSettings::setBatteryLowLevel( lowSpin->value() );
    PowerDevilSettings::setBatteryWarningLevel( warningSpin->value() );
    PowerDevilSettings::setBatteryCriticalLevel( criticalSpin->value() );

    PowerDevilSettings::setBatLowAction( BatteryCriticalCombo->itemData( BatteryCriticalCombo->currentIndex() ).toInt() );

    PowerDevilSettings::setACProfile( acProfile->currentText() );
    PowerDevilSettings::setLowProfile( lowProfile->currentText() );
    PowerDevilSettings::setWarningProfile( warningProfile->currentText() );
    PowerDevilSettings::setBatteryProfile( batteryProfile->currentText() );

    PowerDevilSettings::self()->writeConfig();
}

void ConfigWidget::emitChanged()
{
    emit changed( true );
}

void ConfigWidget::enableBoxes()
{
    displayIdleTime->setEnabled( offDisplayWhenIdle->isChecked() );
}

void ConfigWidget::loadProfile()
{
    kDebug() << "Loading a profile";

    if ( !profilesList->currentItem() )
        return;

    kDebug() << profilesList->currentItem()->text();

    KConfigGroup *group = new KConfigGroup( m_profilesConfig, profilesList->currentItem()->text() );

    if ( !group->isValid() ) {
        delete group;
        return;
    }
    kDebug() << "Ok, KConfigGroup ready";

    kDebug() << group->readEntry( "brightness" );

    brightnessSlider->setValue( group->readEntry( "brightness" ).toInt() );
    offDisplayWhenIdle->setChecked( group->readEntry( "turnOffIdle", false ) );
    displayIdleTime->setValue( group->readEntry( "turnOffIdleTime" ).toInt() );
    idleTime->setValue( group->readEntry( "idleTime" ).toInt() );
    idleCombo->setCurrentIndex( idleCombo->findData( group->readEntry( "idleAction" ).toInt() ) );
    freqCombo->setCurrentIndex( freqCombo->findData( group->readEntry( "cpuPolicy" ).toInt() ) );
    schemeCombo->setCurrentIndex( schemeCombo->findText( group->readEntry( "scheme" ) ) );
    scriptRequester->setPath( group->readEntry( "scriptpath" ) );

    laptopClosedCombo->setCurrentIndex( laptopClosedCombo->findData( group->readEntry( "lidAction" ).toInt() ) );

    QVariant var = group->readEntry( "disabledCPUs", QVariant() );
    QList<QVariant> list = var.toList();

    foreach( const QVariant &ent, list ) {
        QCheckBox *box = qobject_cast<QCheckBox*> ( CPUListLayout->itemAt( ent.toInt() )->widget() );

        if ( !box )
            continue;

        box->setChecked( true );
    }

    delete group;

    m_profileEdited = false;
    enableSaveProfile();
}

void ConfigWidget::saveProfile( const QString &p )
{
    if ( !profilesList->currentItem() && p.isEmpty() ) {
        return;
    }

    QString profile;

    if ( p.isEmpty() ) {
        profile = profilesList->currentItem()->text();
    } else {
        profile = p;
    }

    KConfigGroup *group = new KConfigGroup( m_profilesConfig, profile );

    if ( !group->isValid() || !group->entryMap().size() ) {
        delete group;
        return;
    }

    group->writeEntry( "brightness", brightnessSlider->value() );
    group->writeEntry( "cpuPolicy", freqCombo->itemData( freqCombo->currentIndex() ).toInt() );
    group->writeEntry( "idleAction", idleCombo->itemData( idleCombo->currentIndex() ).toInt() );
    group->writeEntry( "idleTime", idleTime->value() );
    group->writeEntry( "lidAction", laptopClosedCombo->itemData( laptopClosedCombo->currentIndex() ).toInt() );
    group->writeEntry( "turnOffIdle", offDisplayWhenIdle->isChecked() );
    group->writeEntry( "turnOffIdleTime", displayIdleTime->value() );
    group->writeEntry( "scheme", schemeCombo->currentText() );
    group->writeEntry( "scriptpath", scriptRequester->url().path() );

    QList<int> list;

    for ( int i = 0;i < CPUListLayout->count();i++ ) {
        QCheckBox *box = qobject_cast<QCheckBox*> ( CPUListLayout->itemAt( i )->widget() );

        if ( !box )
            continue;

        if ( box->isChecked() )
            list.append( i );
    }

    group->writeEntry( "disabledCPUs", list );

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

    if ( m_profilesConfig->groupList().isEmpty() ) {
        kDebug() << "No available profiles!";
        return;
    }

    foreach( const QString &ent, m_profilesConfig->groupList() ) {
        KConfigGroup *group = new KConfigGroup( m_profilesConfig, ent );
        QListWidgetItem *itm = new QListWidgetItem( KIcon( group->readEntry( "iconname" ) ),
                ent );
        profilesList->addItem( itm );
        acProfile->addItem( KIcon( group->readEntry( "iconname" ) ), ent );
        batteryProfile->addItem( KIcon( group->readEntry( "iconname" ) ), ent );
        lowProfile->addItem( KIcon( group->readEntry( "iconname" ) ), ent );
        warningProfile->addItem( KIcon( group->readEntry( "iconname" ) ), ent );
    }

    acProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::aCProfile() ) );
    lowProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::lowProfile() ) );
    warningProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::warningProfile() ) );
    batteryProfile->setCurrentIndex( acProfile->findText( PowerDevilSettings::batteryProfile() ) );

    profilesList->setCurrentRow( 0 );
}

void ConfigWidget::deleteCurrentProfile()
{
    if ( !profilesList->currentItem() || profilesList->currentItem()->text().isEmpty() )
        return;

    m_profilesConfig->deleteGroup( profilesList->currentItem()->text() );

    m_profilesConfig->sync();

    delete m_profilesConfig;

    m_profilesConfig = new KConfig( "powerdevilprofilesrc", KConfig::SimpleConfig );

    reloadAvailableProfiles();

    emit profilesChanged();
}

void ConfigWidget::createProfile( const QString &name, const QString &icon )
{
    if ( name.isEmpty() )
        return;
    KConfigGroup *group = new KConfigGroup( m_profilesConfig, name );

    group->writeEntry( "brightness", 80 );
    group->writeEntry( "cpuPolicy", ( int ) Solid::Control::PowerManager::Powersave );
    group->writeEntry( "idleAction", 0 );
    group->writeEntry( "idleTime", 50 );
    group->writeEntry( "lidAction", 0 );
    group->writeEntry( "turnOffIdle", false );
    group->writeEntry( "turnOffIdleTime", 50 );
    group->writeEntry( "iconname", icon );

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void ConfigWidget::createProfile()
{
    KDialog *dialog = new KDialog( this );
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit( wg );
    QLabel *lb = new QLabel( wg );
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton( wg );

    ibt->setIconSize( KIconLoader::SizeSmall );

    lb->setText( i18n( "Please enter a name for the new profile" ) );

    lb->setToolTip( i18n( "The name for the new profile" ) );
    lb->setWhatsThis( i18n( "Enter here the name for the profile you are creating" ) );

    ed->setToolTip( i18n( "The name for the new profile" ) );
    ed->setWhatsThis( i18n( "Enter here the name for the profile you are creating" ) );

    lay->addRow( lb );
    lay->addRow( ibt, ed );

    wg->setLayout( lay );

    dialog->setMainWidget( wg );
    ed->setFocus();

    if ( dialog->exec() == KDialog::Accepted ) {
        createProfile( ed->text(), ibt->icon() );
    }
    delete dialog;
}

void ConfigWidget::editProfile( const QString &prevname, const QString &icon )
{
    if ( prevname.isEmpty() )
        return;

    KConfigGroup *group = new KConfigGroup( m_profilesConfig, prevname );

    group->writeEntry( "iconname", icon );

    group->sync();

    delete group;

    reloadAvailableProfiles();

    emit profilesChanged();
}

void ConfigWidget::editProfile()
{
    if ( !profilesList->currentItem() )
        return;

    KDialog *dialog = new KDialog( this );
    QWidget *wg = new QWidget();
    KLineEdit *ed = new KLineEdit( wg );
    QLabel *lb = new QLabel( wg );
    QFormLayout *lay = new QFormLayout();
    KIconButton *ibt = new KIconButton( wg );

    ibt->setIconSize( KIconLoader::SizeSmall );

    lb->setText( i18n( "Please enter a name for this profile" ) );

    lb->setToolTip( i18n( "The name for the new profile" ) );
    lb->setWhatsThis( i18n( "Enter here the name for the profile you are creating" ) );

    ed->setToolTip( i18n( "The name for the new profile" ) );
    ed->setWhatsThis( i18n( "Enter here the name for the profile you are creating" ) );
    ed->setEnabled( false );

    ed->setText( profilesList->currentItem()->text() );

    KConfigGroup *group = new KConfigGroup( m_profilesConfig, profilesList->currentItem()->text() );

    ibt->setIcon( group->readEntry( "iconname" ) );

    lay->addRow( lb );
    lay->addRow( ibt, ed );

    wg->setLayout( lay );

    dialog->setMainWidget( wg );
    ed->setFocus();

    if ( dialog->exec() == KDialog::Accepted ) {
        editProfile( profilesList->currentItem()->text(), ibt->icon() );
    }

    delete dialog;
    delete group;
}

void ConfigWidget::fillCapabilities()
{
    int batteryCount = 0;
    int cpuCount = 0;

    foreach( const Solid::Device &device, Solid::Device::listFromType( Solid::DeviceInterface::Battery, QString() ) ) {
        Q_UNUSED( device )
        batteryCount++;
    }

    foreach( const Solid::Device &device, Solid::Device::listFromType( Solid::DeviceInterface::Processor, QString() ) ) {
        /*Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*>(d.asDeviceInterface(Solid::DeviceInterface::Processor));

        if (processor->canChangeFrequency()) {
            freqchange = true;
        }*/
        Q_UNUSED( device )
        cpuCount++;
    }

    cpuNumber->setText( QString::number( cpuCount ) );
    batteriesNumber->setText( QString::number( batteryCount ) );

    bool turnOff = false;

    for ( int i = 0;i < cpuCount;i++ ) {
        if ( Solid::Control::PowerManager::canDisableCpu( i ) )
            turnOff = true;
    }

    if ( turnOff )
        isCPUOffSupported->setPixmap( KIcon( "dialog-ok-apply" ).pixmap( 16, 16 ) );
    else
        isCPUOffSupported->setPixmap( KIcon( "dialog-cancel" ).pixmap( 16, 16 ) );

    QString sMethods;

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if ( methods & Solid::Control::PowerManager::ToDisk ) {
        sMethods.append( QString( i18n( "Suspend to Disk" ) + QString( ", " ) ) );
    }

    if ( methods & Solid::Control::PowerManager::ToRam ) {
        sMethods.append( QString( i18n( "Suspend to RAM" ) + QString( ", " ) ) );
    }

    if ( methods & Solid::Control::PowerManager::Standby ) {
        sMethods.append( QString( i18n( "Standby" ) + QString( ", " ) ) );
    }

    if ( !sMethods.isEmpty() ) {
        sMethods.remove( sMethods.length() - 2, 2 );
    } else {
        sMethods = i18nc( "None", "No methods found" );
    }

    supportedMethods->setText( sMethods );

    QString scMethods;

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if ( policies & Solid::Control::PowerManager::Performance ) {
        scMethods.append( QString( i18n( "Performance" ) + QString( ", " ) ) );
    }

    if ( policies & Solid::Control::PowerManager::OnDemand ) {
        scMethods.append( QString( i18n( "Dynamic (ondemand)" ) + QString( ", " ) ) );
    }

    if ( policies & Solid::Control::PowerManager::Conservative ) {
        scMethods.append( QString( i18n( "Dynamic (conservative)" ) + QString( ", " ) ) );
    }

    if ( policies & Solid::Control::PowerManager::Powersave ) {
        scMethods.append( QString( i18n( "Powersave" ) + QString( ", " ) ) );
    }

    if ( policies & Solid::Control::PowerManager::Userspace ) {
        scMethods.append( QString( i18n( "Userspace" ) + QString( ", " ) ) );
    }

    if ( !scMethods.isEmpty() ) {
        scMethods.remove( scMethods.length() - 2, 2 );
        isScalingSupported->setPixmap( KIcon( "dialog-ok-apply" ).pixmap( 16, 16 ) );
    } else {
        scMethods = i18nc( "None", "No methods found" );
        isScalingSupported->setPixmap( KIcon( "dialog-cancel" ).pixmap( 16, 16 ) );
    }

    supportedPolicies->setText( scMethods );

    if ( !Solid::Control::PowerManager::supportedSchemes().isEmpty() ) {
        isSchemeSupported->setPixmap( KIcon( "dialog-ok-apply" ).pixmap( 16, 16 ) );
    } else {
        isSchemeSupported->setPixmap( KIcon( "dialog-cancel" ).pixmap( 16, 16 ) );
    }

    QString schemes;

    foreach( const QString &scheme, Solid::Control::PowerManager::supportedSchemes() ) {
        schemes.append( scheme + QString( ", " ) );
    }

    if ( !schemes.isEmpty() ) {
        schemes.remove( schemes.length() - 2, 2 );
    } else {
        schemes = i18nc( "None", "No methods found" );
    }

    supportedSchemes->setText( schemes );
}

void ConfigWidget::importProfiles()
{
    QString fileName = KFileDialog::getOpenFileName( KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n( "Import PowerDevil profiles" ) );

    if ( fileName.isEmpty() ) {
        return;
    }

    KConfig toImport( fileName, KConfig::SimpleConfig );

    // FIXME: This should be the correct way, but why it doesn't work?
    /*foreach(const QString &ent, toImport.groupList()) {
        KConfigGroup group = toImport.group(ent);

        group.copyTo(m_profilesConfig);
    }*/

    // FIXME: This way works, but the import file gets cleared, definitely not what we want
    foreach( const QString &ent, toImport.groupList() ) {
        KConfigGroup group = toImport.group( ent );
        KConfigGroup group2( group );

        group2.reparent( m_profilesConfig );
    }

    m_profilesConfig->sync();

    reloadAvailableProfiles();

    emit profilesChanged();
}

void ConfigWidget::exportProfiles()
{
    QString fileName = KFileDialog::getSaveFileName( KUrl(), "*.powerdevilprofiles|PowerDevil Profiles "
                       "(*.powerdevilprofiles)", this, i18n( "Export PowerDevil profiles" ) );

    if ( fileName.isEmpty() ) {
        return;
    }

    kDebug() << "Filename is" << fileName;

    KConfig *toExport = m_profilesConfig->copyTo( fileName );

    toExport->sync();

    delete toExport;
}

void ConfigWidget::switchProfile( QListWidgetItem *current, QListWidgetItem *previous )
{
    Q_UNUSED( current )

    if ( !m_profileEdited ) {
        loadProfile();
    } else {
        int result = KMessageBox::warningYesNoCancel( this, i18n( "The current profile has not been saved.\n"
                     "Do you want to save it?" ), i18n( "Save Profile" ) );

        if ( result == KMessageBox::Yes ) {
            saveProfile( previous->text() );
            loadProfile();
        } else if ( result == KMessageBox::No ) {
            loadProfile();
        } else if ( result == KMessageBox::Cancel ) {
            disconnect( profilesList, SIGNAL( currentItemChanged( QListWidgetItem*, QListWidgetItem* ) ),
                        this, SLOT( switchProfile( QListWidgetItem*, QListWidgetItem* ) ) );
            profilesList->setCurrentItem( previous );
            connect( profilesList, SIGNAL( currentItemChanged( QListWidgetItem*, QListWidgetItem* ) ),
                     SLOT( switchProfile( QListWidgetItem*, QListWidgetItem* ) ) );
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
    saveCurrentProfileButton->setEnabled( m_profileEdited );
}

#include "ConfigWidget.moc"
