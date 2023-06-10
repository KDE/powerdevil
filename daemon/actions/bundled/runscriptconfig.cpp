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


#include "runscriptconfig.h"

#include <QHBoxLayout>
#include <QSpinBox>

#include <KConfig>
#include <KLocalizedString>
#include <KUrlRequester>
#include <KComboBox>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_FACTORY(PowerDevilRunScriptConfigFactory, registerPlugin<PowerDevil::BundledActions::RunScriptConfig>(); )

namespace PowerDevil {
namespace BundledActions {

RunScriptConfig::RunScriptConfig(QObject* parent)
    : ActionConfig(parent)
{

}

RunScriptConfig::~RunScriptConfig()
{

}

void RunScriptConfig::save()
{
    configGroup().writeEntry("scriptCommand", m_urlRequester->text());
    configGroup().writeEntry("scriptPhase", m_comboBox->currentIndex());
    configGroup().writeEntry("idleTime", m_idleTime->value() * 60 * 1000);

    configGroup().sync();
}

void RunScriptConfig::load()
{
    configGroup().config()->reparseConfiguration();
    m_urlRequester->setText(configGroup().readEntry<QString>("scriptCommand", QString()));
    m_comboBox->setCurrentIndex(configGroup().readEntry<int>("scriptPhase", 0));
    m_idleTime->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > RunScriptConfig::buildUi()
{
    QList< QPair< QString, QWidget* > > retlist;
    m_urlRequester = new KUrlRequester();
    m_urlRequester->setMode(KFile::File | KFile::LocalOnly | KFile::ExistingOnly);
    retlist.append(qMakePair(i18n("Script"), m_urlRequester));

    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new QSpinBox;
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setMinimum(1);
    m_idleTime->setMaximum(360);
    m_idleTime->setValue(0);
    m_idleTime->setDisabled(true);
    m_idleTime->setSuffix(i18n(" min"));
    m_comboBox->addItem(i18n("On Profile Load"));
    m_comboBox->addItem(i18n("On Profile Unload"));
    m_comboBox->addItem(i18n("After"));
    connect(m_comboBox, &KComboBox::currentIndexChanged, this, [this](int index) {
        onIndexChanged(m_comboBox->itemText(index));
    });

    hlay->addWidget(m_comboBox);
    hlay->addWidget(m_idleTime);
    hlay->addStretch();

    tempWidget->setLayout(hlay);

    retlist.append(qMakePair(i18n("Run script"), tempWidget));

    connect(m_urlRequester, &KUrlRequester::textChanged, this, &RunScriptConfig::setChanged);
    connect(m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_idleTime, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}

void RunScriptConfig::onIndexChanged(const QString &text)
{
    m_idleTime->setEnabled(text == i18n("After"));
}

}
}

#include "runscriptconfig.moc"
