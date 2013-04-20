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

#include "keyboardbrightnesscontrolconfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include <KPluginFactory>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilKeyboardBrightnessControlConfigFactory, registerPlugin<PowerDevil::BundledActions::KeyboardBrightnessControlConfig>(); )
K_EXPORT_PLUGIN(PowerDevilKeyboardBrightnessControlConfigFactory("powerdevilkeyboardbrightnesscontrolaction_config"))

namespace PowerDevil {
namespace BundledActions {

KeyboardBrightnessControlConfig::KeyboardBrightnessControlConfig(QObject *parent, const QVariantList& )
    : ActionConfig(parent)
{

}

KeyboardBrightnessControlConfig::~KeyboardBrightnessControlConfig()
{

}

void KeyboardBrightnessControlConfig::save()
{
    configGroup().writeEntry("value", m_slider->value());
    configGroup().sync();
}

void KeyboardBrightnessControlConfig::load()
{
    configGroup().config()->reparseConfiguration();
    m_slider->setValue(configGroup().readEntry<int>("value", 50));
}

QList< QPair< QString, QWidget* > > KeyboardBrightnessControlConfig::buildUi()
{
    QList< QPair< QString, QWidget* > > retlist;
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setMaximumWidth(300);
    m_slider->setRange(0, 100);
    retlist.append(qMakePair< QString, QWidget* >(i18nc("@label:slider Brightness level", "Level"), m_slider));

    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}


}
}

#include "keyboardbrightnesscontrolconfig.moc"
