/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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
#include "upower_interface.h"

#include <Solid/Device>
#include <Solid/PowerManagement>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QComboBox>
#include <QCheckBox>
#include <QIcon>

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
        configGroup().writeEntry<uint>("lidAction", m_lidCloseCombo->itemData(m_lidCloseCombo->currentIndex()).toUInt());
    }
    if (m_triggerLidActionWhenExternalMonitorPresent) {
        configGroup().writeEntry<bool>("triggerLidActionWhenExternalMonitorPresent", m_triggerLidActionWhenExternalMonitorPresent->isChecked());
    }
    if (m_powerButtonCombo) {
        configGroup().writeEntry<uint>("powerButtonAction", m_powerButtonCombo->itemData(m_powerButtonCombo->currentIndex()).toUInt());
    }

    configGroup().sync();
}

void HandleButtonEventsConfig::load()
{
    configGroup().config()->reparseConfiguration();

    if (m_lidCloseCombo) {
        m_lidCloseCombo->setCurrentIndex(m_lidCloseCombo->findData(QVariant::fromValue(configGroup().readEntry<uint>("lidAction", 0))));
    }
    if (m_triggerLidActionWhenExternalMonitorPresent) {
        m_triggerLidActionWhenExternalMonitorPresent->setChecked(configGroup().readEntry<bool>("triggerLidActionWhenExternalMonitorPresent", false));
    }
    if (m_powerButtonCombo) {
        m_powerButtonCombo->setCurrentIndex(m_powerButtonCombo->findData(QVariant::fromValue(configGroup().readEntry<uint>("powerButtonAction", 0))));
    }
}

QList< QPair< QString, QWidget* > > HandleButtonEventsConfig::buildUi()
{
    // Create the boxes
    m_lidCloseCombo = new QComboBox;
    m_triggerLidActionWhenExternalMonitorPresent = new QCheckBox(
                i18nc("Execute action on lid close even when external monitor is connected", "Even when an external monitor is connected")
    );
    m_powerButtonCombo = new QComboBox;

    // Fill the boxes with options!
    {
        QList<QComboBox *> boxes;
        boxes << m_lidCloseCombo << m_powerButtonCombo;

        QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

        foreach (QComboBox *box, boxes) {
            box->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), (uint)SuspendSession::None);
            if (methods.contains(Solid::PowerManagement::SuspendState)) {
                box->addItem(QIcon::fromTheme("system-suspend"), i18n("Suspend"), (uint)SuspendSession::ToRamMode);
            }
            if (methods.contains(Solid::PowerManagement::HibernateState)) {
                box->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
            }
            box->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shutdown"), (uint)SuspendSession::ShutdownMode);
            box->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), (uint)SuspendSession::LockScreenMode);
            if (box != m_lidCloseCombo) {
                box->addItem(QIcon::fromTheme("system-log-out"), i18n("Prompt log out dialog"), (uint)SuspendSession::LogoutDialogMode);
            }
            box->addItem(QIcon::fromTheme("preferences-desktop-screensaver"), i18n("Turn off screen"), (uint)SuspendSession::TurnOffScreenMode);
        }
    }

    connect(m_lidCloseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_triggerLidActionWhenExternalMonitorPresent, SIGNAL(stateChanged(int)), this, SLOT(setChanged()));
    connect(m_powerButtonCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));

    bool lidFound = false;
    bool powerFound = true; // HACK This needs proper API!!
    // get a list of all devices that are Buttons
/*    foreach (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Button, QString())) {
        Solid::Button *button = device.as<Solid::Button>();
        if (button->type() == Solid::Button::LidButton) {
            lidFound = true;
        } else if (button->type() == Solid::Button::PowerButton) {
            powerFound = true;
        }
    }*/

    auto m_upowerInterface = new OrgFreedesktopUPowerInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);

    lidFound = m_upowerInterface->lidIsPresent();
    QList< QPair< QString, QWidget* > > retlist;

    if (lidFound) {
        retlist.append(qMakePair<QString, QWidget *>(i18n("When laptop lid closed"), m_lidCloseCombo));
        // an empty label will make it treat the widget as title checkbox and left-align it
        retlist.append(qMakePair<QString, QWidget *>(QLatin1Literal("NONE"), m_triggerLidActionWhenExternalMonitorPresent));
    } else {
        m_lidCloseCombo->deleteLater();
        m_lidCloseCombo = nullptr;
        m_triggerLidActionWhenExternalMonitorPresent->deleteLater();
        m_triggerLidActionWhenExternalMonitorPresent = nullptr;
    }

    if (powerFound) {
        retlist.append(qMakePair< QString, QWidget* >(i18n("When power button pressed"), m_powerButtonCombo));
    } else {
        m_powerButtonCombo->deleteLater();
        m_powerButtonCombo = nullptr;
    }

    // unified width for the comboboxes
    int comboBoxMaxWidth = 300;
    if (m_lidCloseCombo) {
        comboBoxMaxWidth = qMax(comboBoxMaxWidth, m_lidCloseCombo->sizeHint().width());
    }
    if (m_powerButtonCombo) {
        comboBoxMaxWidth = qMax(comboBoxMaxWidth, m_powerButtonCombo->sizeHint().width());
    }
    if (m_lidCloseCombo) {
        m_lidCloseCombo->setMinimumWidth(300);
        m_lidCloseCombo->setMaximumWidth(comboBoxMaxWidth);
    }
    if (m_powerButtonCombo) {
        m_powerButtonCombo->setMinimumWidth(300);
        m_powerButtonCombo->setMaximumWidth(comboBoxMaxWidth);
    }

    return retlist;
}

}
}

#include "handlebuttoneventsconfig.moc"
