/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#include "handlebuttoneventsconfig.h"

#include "suspendsession.h"

#include <Solid/Button>
#include <Solid/Device>
#include <Solid/PowerManagement>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KComboBox>
#include <KIcon>

K_PLUGIN_FACTORY(PowerDevilSuspendSessionConfigFactory, registerPlugin<PowerDevil::BundledActions::HandleButtonEventsConfig>(); )

namespace PowerDevil {
namespace BundledActions {

HandleButtonEventsConfig::HandleButtonEventsConfig(QObject* parent, const QVariantList& )
    : ActionConfig(parent)
{

}

HandleButtonEventsConfig::~HandleButtonEventsConfig()
{

}


void HandleButtonEventsConfig::save()
{
    if (m_lidCloseCombo) {
        configGroup().writeEntry< uint >("lidAction", m_lidCloseCombo->itemData(m_lidCloseCombo->currentIndex()).toUInt());
    }
    if (m_powerButtonCombo) {
        configGroup().writeEntry< uint >("powerButtonAction", m_powerButtonCombo->itemData(m_powerButtonCombo->currentIndex()).toUInt());
    }

    configGroup().sync();
}

void HandleButtonEventsConfig::load()
{
    configGroup().config()->reparseConfiguration();

    if (m_lidCloseCombo) {
        m_lidCloseCombo->setCurrentIndex(m_lidCloseCombo->findData(QVariant::fromValue(configGroup().readEntry< uint >("lidAction", 0))));
    }
    if (m_powerButtonCombo) {
        m_powerButtonCombo->setCurrentIndex(m_powerButtonCombo->findData(QVariant::fromValue(configGroup().readEntry< uint >("powerButtonAction", 0))));
    }
}

QList< QPair< QString, QWidget* > > HandleButtonEventsConfig::buildUi()
{
    // Create the boxes
    m_lidCloseCombo = new KComboBox;
    m_powerButtonCombo = new KComboBox;

    // Fill the boxes with options!
    {
        QList< KComboBox* > boxes;
        boxes << m_lidCloseCombo << m_powerButtonCombo;

        QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

        foreach (KComboBox *box, boxes) {
            box->addItem(KIcon("dialog-cancel"), i18n("Do nothing"), (uint)SuspendSession::None);
            if (methods.contains(Solid::PowerManagement::SuspendState)) {
                box->addItem(KIcon("system-suspend"), i18n("Sleep"), (uint)SuspendSession::ToRamMode);
            }
            if (methods.contains(Solid::PowerManagement::HibernateState)) {
                box->addItem(KIcon("system-suspend-hibernate"), i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
            }
            box->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (uint)SuspendSession::ShutdownMode);
            box->addItem(KIcon("system-lock-screen"), i18n("Lock screen"), (uint)SuspendSession::LockScreenMode);
            if (box != m_lidCloseCombo) {
                box->addItem(KIcon("system-log-out"), i18n("Prompt log out dialog"), (uint)SuspendSession::LogoutDialogMode);
            }
            box->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn off screen"), (uint)SuspendSession::TurnOffScreenMode);
        }
    }

    connect(m_lidCloseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_powerButtonCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));

    m_lidCloseCombo->setMaximumWidth(300);
    m_powerButtonCombo->setMaximumWidth(300);

    bool lidFound = false;
    bool powerFound = false;
    // get a list of all devices that are Buttons
    foreach (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Button, QString())) {
        Solid::Button *button = device.as<Solid::Button>();
        if (button->type() == Solid::Button::LidButton) {
            lidFound = true;
        } else if (button->type() == Solid::Button::PowerButton) {
            powerFound = true;
        }
    }

    QList< QPair< QString, QWidget* > > retlist;

    if (lidFound) {
        retlist.append(qMakePair< QString, QWidget* >(i18n("When laptop lid closed"), m_lidCloseCombo));
    } else {
        m_lidCloseCombo->deleteLater();
        m_lidCloseCombo = 0;
    }

    if (powerFound) {
        retlist.append(qMakePair< QString, QWidget* >(i18n("When power button pressed"), m_powerButtonCombo));
    } else {
        m_powerButtonCombo->deleteLater();
        m_powerButtonCombo = 0;
    }

    return retlist;
}

}
}

#include "handlebuttoneventsconfig.moc"
