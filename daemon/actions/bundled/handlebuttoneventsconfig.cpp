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

#include <KLocalizedString>
#include <KPluginFactory>
#include <KComboBox>
#include <KIcon>
#include <solid/powermanagement.h>

K_PLUGIN_FACTORY(PowerDevilSuspendSessionConfigFactory, registerPlugin<PowerDevil::BundledActions::HandleButtonEventsConfig>(); )
K_EXPORT_PLUGIN(PowerDevilSuspendSessionConfigFactory("powerdevilhandlebuttoneventsaction_config"))

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
    configGroup().writeEntry< uint >("lidAction", m_lidCloseCombo->itemData(m_lidCloseCombo->currentIndex()).toUInt());
    configGroup().writeEntry< uint >("powerButtonAction", m_powerButtonCombo->itemData(m_powerButtonCombo->currentIndex()).toUInt());
    configGroup().writeEntry< uint >("sleepButtonAction", m_sleepButtonCombo->itemData(m_sleepButtonCombo->currentIndex()).toUInt());
}

void HandleButtonEventsConfig::load()
{
    m_lidCloseCombo->setCurrentIndex(configGroup().readEntry< uint >("lidAction", 0));
    m_powerButtonCombo->setCurrentIndex(configGroup().readEntry< uint >("powerButtonAction", 0));
    m_sleepButtonCombo->setCurrentIndex(configGroup().readEntry< uint >("sleepButtonAction", 0));
}

QList< QPair< QString, QWidget* > > HandleButtonEventsConfig::buildUi()
{
    // Create the boxes
    m_lidCloseCombo = new KComboBox;
    m_sleepButtonCombo = new KComboBox;
    m_powerButtonCombo = new KComboBox;

    // Fill the boxes with options!
    {
        QList< KComboBox* > boxes;
        boxes << m_lidCloseCombo << m_powerButtonCombo << m_sleepButtonCombo;

        QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

        foreach (KComboBox *box, boxes) {
            box->addItem(KIcon("dialog-cancel"), i18n("Do nothing"), (uint)0);
            if (methods.contains(Solid::PowerManagement::SuspendState)) {
                box->addItem(KIcon("system-suspend"), i18n("Sleep"), (uint)1);
            }
            if (methods.contains(Solid::PowerManagement::HibernateState)) {
                box->addItem(KIcon("system-suspend-hibernate"), i18n("Hibernate"), (uint)2);
            }
            box->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (uint)3);
            box->addItem(KIcon("system-lock-screen"), i18n("Lock screen"), (uint)4);
            if (box != m_lidCloseCombo) {
                box->addItem(KIcon("system-log-out"), i18n("Prompt log out dialog"), (uint)5);
            }
            box->addItem(KIcon("preferences-desktop-screensaver"), i18n("Turn off screen"), (uint)6);
        }
    }

    connect(m_lidCloseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_sleepButtonCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_powerButtonCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));

    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair< QString, QWidget* >(i18n("When laptop lid closed"), m_lidCloseCombo));
    retlist.append(qMakePair< QString, QWidget* >(i18n("When power button pressed"), m_powerButtonCombo));
    retlist.append(qMakePair< QString, QWidget* >(i18n("When sleep button pressed"), m_sleepButtonCombo));

    return retlist;
}

}
}

#include "handlebuttoneventsconfig.moc"
