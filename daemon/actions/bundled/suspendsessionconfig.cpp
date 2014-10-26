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


#include "suspendsessionconfig.h"

#include <QHBoxLayout>
#include <QSpinBox>

#include <Solid/PowerManagement>

#include <KComboBox>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QIcon>
#include <KConfig>
#include "suspendsession.h"

K_PLUGIN_FACTORY(PowerDevilSuspendSessionConfigFactory, registerPlugin<PowerDevil::BundledActions::SuspendSessionConfig>(); )

namespace PowerDevil {
namespace BundledActions {

SuspendSessionConfig::SuspendSessionConfig(QObject* parent, const QVariantList&)
        : ActionConfig(parent)
{

}

SuspendSessionConfig::~SuspendSessionConfig()
{

}

void SuspendSessionConfig::save()
{
    configGroup().writeEntry< uint >("suspendType", m_comboBox->itemData(m_comboBox->currentIndex()).toUInt());
    configGroup().writeEntry("idleTime", m_idleTime->value() * 60 * 1000);

    configGroup().sync();
}

void SuspendSessionConfig::load()
{
    configGroup().config()->reparseConfiguration();

    uint suspendType = configGroup().readEntry< uint >("suspendType", 0);
    m_comboBox->setCurrentIndex(m_comboBox->findData(suspendType));
    m_idleTime->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > SuspendSessionConfig::buildUi()
{
    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new QSpinBox;
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setMinimum(1);
    m_idleTime->setMaximum(360);
    m_idleTime->setValue(0);
    m_idleTime->setSuffix(i18n(" min"));

    QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

    if (methods.contains(Solid::PowerManagement::SuspendState)) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend"), i18n("Sleep"), (uint)SuspendSession::ToRamMode);
    }
    if (methods.contains(Solid::PowerManagement::HibernateState)) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
    }
    m_comboBox->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shutdown"), (uint)SuspendSession::ShutdownMode);
    m_comboBox->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), (uint)SuspendSession::LockScreenMode);

    hlay->addWidget(m_idleTime);
    hlay->addWidget(m_comboBox);
    hlay->addStretch();

    tempWidget->setLayout(hlay);

    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair< QString, QWidget* >(i18n("After"), tempWidget));

    connect(m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_idleTime, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}

}
}

#include "suspendsessionconfig.moc"
