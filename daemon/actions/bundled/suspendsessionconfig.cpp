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
#include <KComboBox>
#include <KIntSpinBox>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_FACTORY(PowerDevilSuspendSessionConfigFactory, registerPlugin<PowerDevil::BundledActions::SuspendSessionConfig>(); )
K_EXPORT_PLUGIN(PowerDevilSuspendSessionConfigFactory("powerdevilsuspendsessionaction_config"))

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
    switch (m_comboBox->currentIndex()) {
        case 0:
            configGroup().writeEntry("suspendType", "Suspend");
            break;
        case 1:
            configGroup().writeEntry("suspendType", "ToDisk");
            break;
        case 2:
            configGroup().writeEntry("suspendType", "Shutdown");
            break;
        default:
            break;
    }
    configGroup().writeEntry("idleTime", m_idleTime->value() * 60 * 1000);

    configGroup().sync();
}

void SuspendSessionConfig::load()
{
    QString suspendType = configGroup().readEntry< QString >("suspendType", QString());
    if (suspendType == "Suspend") {
        m_comboBox->setCurrentIndex(0);
    } else if (suspendType == "ToDisk") {
        m_comboBox->setCurrentIndex(1);
    } else if (suspendType == "Shutdown") {
        m_comboBox->setCurrentIndex(2);
    }
    m_idleTime->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > SuspendSessionConfig::buildUi()
{
    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new KIntSpinBox(0, 180, 1, 0, 0);
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setSuffix(i18n(" min"));
    m_comboBox->addItem(i18n("Sleep"));
    m_comboBox->addItem(i18n("Hibernate"));
    m_comboBox->addItem(i18n("Shutdown"));

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
