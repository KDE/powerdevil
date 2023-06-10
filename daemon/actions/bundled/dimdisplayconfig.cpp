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

#include "dimdisplayconfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

#include <KConfig>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilDimDisplayConfigFactory, registerPlugin<PowerDevil::BundledActions::DimDisplayConfig>(); )

namespace PowerDevil {
namespace BundledActions {

DimDisplayConfig::DimDisplayConfig(QObject *parent)
    : ActionConfig(parent)
{

}

DimDisplayConfig::~DimDisplayConfig()
{

}

void DimDisplayConfig::save()
{
    configGroup().writeEntry("idleTime", m_spinBox->value() * 60 * 1000);
}

void DimDisplayConfig::load()
{
    configGroup().config()->reparseConfiguration();
    m_spinBox->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > DimDisplayConfig::buildUi()
{
    m_spinBox = new QSpinBox;
    m_spinBox->setMaximumWidth(150);
    m_spinBox->setMinimum(1);
    m_spinBox->setMaximum(360);
    m_spinBox->setSuffix(i18n(" min"));

    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    tempWidget->setLayout(hlay);

    hlay->addWidget(m_spinBox);
    hlay->addStretch();

    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair(i18n("After"), tempWidget));

    connect(m_spinBox, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}

}
}

#include "dimdisplayconfig.moc"
