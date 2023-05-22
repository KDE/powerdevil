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

#include <KConfig>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY(PowerDevilBrightnessControlConfigFactory, registerPlugin<PowerDevil::BundledActions::BrightnessControlConfig>(); )

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
    configGroup().config()->reparseConfiguration();
    m_slider->setValue(configGroup().readEntry<int>("value", 50));
}

QList< QPair< QString, QWidget* > > BrightnessControlConfig::buildUi()
{
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, 100);
    
    m_text = new QLabel("0%");
    connect(m_slider, &QSlider::valueChanged, m_text, [this](int percentage) {
        m_text->setText(QString::number(percentage).append("%"));
    });

    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    hlay->addWidget(m_slider);
    hlay->addWidget(m_text);
    tempWidget->setLayout(hlay);
    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair(i18nc("Brightness level, label for the slider", "Level"), tempWidget));

    connect(m_slider, &QAbstractSlider::valueChanged, this, &BrightnessControlConfig::setChanged);

    return retlist;
}


}
}

#include "brightnesscontrolconfig.moc"
