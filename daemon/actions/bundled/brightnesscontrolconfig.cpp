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

#include "brightnesscontrolconfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include <KPluginFactory>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilBrightnessControlConfigFactory, registerPlugin<PowerDevil::BundledActions::BrightnessControlConfig>(); )
K_EXPORT_PLUGIN(PowerDevilBrightnessControlConfigFactory("powerdevilbrightnesscontrolaction_config"))

namespace PowerDevil {
namespace BundledActions {

BrightnessControlConfig::BrightnessControlConfig(QObject *parent, const QVariantList& )
    : ActionConfig(parent)
{

}

BrightnessControlConfig::~BrightnessControlConfig()
{

}

void BrightnessControlConfig::save()
{
    configGroup().writeEntry("value", m_slider->value());
    configGroup().sync();
}

void BrightnessControlConfig::load()
{
    m_slider->setValue(configGroup().readEntry<int>("value", 50));
}

QList< QPair< QString, QWidget* > > BrightnessControlConfig::buildUi()
{
    QList< QPair< QString, QWidget* > > retlist;
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setMaximumWidth(300);
    m_slider->setRange(0, 100);
    retlist.append(qMakePair< QString, QWidget* >(i18nc("Brightness level, label for the slider", "Level"), m_slider));

    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}


}
}

#include "brightnesscontrolconfig.moc"
