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

#include "CapabilitiesPage.h"

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

CapabilitiesPage::CapabilitiesPage( QWidget *parent )
        : QWidget( parent )
{
    setupUi( this );

    fillUi();
}

CapabilitiesPage::~CapabilitiesPage()
{
}

void CapabilitiesPage::fillUi()
{
    fillCapabilities();
}

void CapabilitiesPage::fillCapabilities()
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

#include "CapabilitiesPage.moc"
