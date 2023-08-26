/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "keyboardbrightnesscontrolconfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_CLASS(PowerDevil::BundledActions::KeyboardBrightnessControlConfig)

namespace PowerDevil::BundledActions
{
KeyboardBrightnessControlConfig::KeyboardBrightnessControlConfig(QObject *parent)
    : ActionConfig(parent)
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

QList<QPair<QString, QWidget *>> KeyboardBrightnessControlConfig::buildUi()
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

    QList<QPair<QString, QWidget *>> retlist;
    retlist.append(qMakePair(i18nc("@label:slider Brightness level", "Level"), tempWidget));

    connect(m_slider, &QAbstractSlider::valueChanged, this, &KeyboardBrightnessControlConfig::setChanged);

    return retlist;
}

}

#include "keyboardbrightnesscontrolconfig.moc"

#include "moc_keyboardbrightnesscontrolconfig.cpp"
