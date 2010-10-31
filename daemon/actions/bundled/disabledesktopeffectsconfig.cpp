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

#include "disabledesktopeffectsconfig.h"

#include <QHBoxLayout>

#include <KComboBox>
#include <KIntSpinBox>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_FACTORY(PowerDevilDisableDesktopEffectsConfigFactory, registerPlugin<PowerDevil::BundledActions::DisableDesktopEffectsConfig>(); )
K_EXPORT_PLUGIN(PowerDevilDisableDesktopEffectsConfigFactory("powerdevildisabledesktopeffectsaction_config"))

namespace PowerDevil {
namespace BundledActions {

DisableDesktopEffectsConfig::DisableDesktopEffectsConfig(QObject* parent, const QVariantList& )
        : ActionConfig(parent)
{

}

DisableDesktopEffectsConfig::~DisableDesktopEffectsConfig()
{

}

void DisableDesktopEffectsConfig::save()
{
    configGroup().writeEntry("onIdle", m_comboBox->currentIndex() == 1);
    configGroup().writeEntry("idleTime", m_idleTime->value() * 60 * 1000);

    configGroup().sync();
}

void DisableDesktopEffectsConfig::load()
{
    m_comboBox->setCurrentIndex(configGroup().readEntry<bool>("onIdle", false) ? 1 : 0);
    m_idleTime->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > DisableDesktopEffectsConfig::buildUi()
{
    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new KIntSpinBox(0, 180, 1, 0, 0);
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setSuffix(i18n(" min"));
    m_idleTime->setDisabled(true);
    m_comboBox->addItem(i18n("On Profile Load"));
    m_comboBox->addItem(i18n("After"));
    connect(m_comboBox, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(onIndexChanged(QString)));

    hlay->addWidget(m_comboBox);
    hlay->addWidget(m_idleTime);
    hlay->addStretch();

    tempWidget->setLayout(hlay);

    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair< QString, QWidget* >(i18n("Disable effects"), tempWidget));

    connect(m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_idleTime, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}

void DisableDesktopEffectsConfig::onIndexChanged(const QString &text)
{
    m_idleTime->setEnabled(text == i18n("After"));
}

}
}

#include "disabledesktopeffectsconfig.moc"
