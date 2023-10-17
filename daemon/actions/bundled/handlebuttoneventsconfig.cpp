/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "handlebuttoneventsconfig.h"

#include "upower_interface.h"

#include <powerdevilenums.h>
#include <powerdevilpowermanagement.h>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QIcon>

K_PLUGIN_CLASS(PowerDevil::BundledActions::HandleButtonEventsConfig)

namespace PowerDevil::BundledActions
{
HandleButtonEventsConfig::HandleButtonEventsConfig(QObject *parent)
    : ActionConfig(parent)
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

QList<QPair<QString, QWidget *>> HandleButtonEventsConfig::buildUi()
{
    // Create the boxes
    m_lidCloseCombo = new QComboBox;
    m_triggerLidActionWhenExternalMonitorPresent =
        new QCheckBox(i18nc("Execute action on lid close even when external monitor is connected", "Even when an external monitor is connected"));
    m_powerButtonCombo = new QComboBox;

    // Fill the boxes with options!
    {
        QList<QComboBox *> boxes;
        boxes << m_lidCloseCombo << m_powerButtonCombo;

        for (QComboBox *box : std::as_const(boxes)) {
            box->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), static_cast<uint>(PowerDevil::PowerButtonAction::NoAction));
            if (PowerManagement::instance()->canSuspend()) {
                box->addItem(QIcon::fromTheme("system-suspend"),
                             i18nc("Suspend to RAM", "Sleep"),
                             static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToRam));
            }
            if (PowerManagement::instance()->canHibernate()) {
                box->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToDisk));
            }
            if (PowerManagement::instance()->canHybridSuspend()) {
                box->addItem(QIcon::fromTheme("system-suspend-hybrid"), i18n("Hybrid sleep"), static_cast<uint>(PowerDevil::PowerButtonAction::SuspendHybrid));
            }
            box->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), static_cast<uint>(PowerDevil::PowerButtonAction::Shutdown));
            box->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), static_cast<uint>(PowerDevil::PowerButtonAction::LockScreen));
            if (box != m_lidCloseCombo) {
                box->addItem(QIcon::fromTheme("system-log-out"),
                             i18n("Prompt log out dialog"),
                             static_cast<uint>(PowerDevil::PowerButtonAction::PromptLogoutDialog));
            }
            box->addItem(QIcon::fromTheme("preferences-desktop-screensaver"),
                         i18n("Turn off screen"),
                         static_cast<uint>(PowerDevil::PowerButtonAction::TurnOffScreen));
        }
    }

    connect(m_lidCloseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_triggerLidActionWhenExternalMonitorPresent, &QCheckBox::stateChanged, this, &HandleButtonEventsConfig::setChanged);
    connect(m_powerButtonCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));

    bool lidFound = false;
    bool powerFound = true; // HACK This needs proper API!!
    // get a list of all devices that are Buttons
    /*    Q_FOREACH (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Button, QString())) {
            Solid::Button *button = device.as<Solid::Button>();
            if (button->type() == Solid::Button::LidButton) {
                lidFound = true;
            } else if (button->type() == Solid::Button::PowerButton) {
                powerFound = true;
            }
        }*/

    auto m_upowerInterface = new OrgFreedesktopUPowerInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);

    lidFound = m_upowerInterface->lidIsPresent();
    QList<QPair<QString, QWidget *>> retlist;

    if (lidFound) {
        retlist.append(qMakePair<QString, QWidget *>(i18n("When laptop lid closed"), m_lidCloseCombo));
        // an empty label will make it treat the widget as title checkbox and left-align it
        retlist.append(qMakePair<QString, QWidget *>(QLatin1String("NONE"), m_triggerLidActionWhenExternalMonitorPresent));
    } else {
        m_lidCloseCombo->deleteLater();
        m_lidCloseCombo = nullptr;
        m_triggerLidActionWhenExternalMonitorPresent->deleteLater();
        m_triggerLidActionWhenExternalMonitorPresent = nullptr;
    }

    if (powerFound) {
        retlist.append(qMakePair<QString, QWidget *>(i18n("When power button pressed"), m_powerButtonCombo));
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

#include "handlebuttoneventsconfig.moc"

#include "moc_handlebuttoneventsconfig.cpp"
