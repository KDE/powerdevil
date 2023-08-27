/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "brightnesscontrolconfig.h"

#include <PowerDevilProfileSettings.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_CLASS(PowerDevil::BundledActions::BrightnessControlConfig)

namespace PowerDevil::BundledActions
{
BrightnessControlConfig::BrightnessControlConfig(QObject *parent)
    : ActionConfig(parent)
{
}

void BrightnessControlConfig::save()
{
    profileSettings()->setDisplayBrightness(m_slider->value());
}

void BrightnessControlConfig::load()
{
    m_slider->setValue(profileSettings()->displayBrightness());
}

bool BrightnessControlConfig::enabledInProfileSettings() const
{
    return profileSettings()->useProfileSpecificDisplayBrightness();
}

void BrightnessControlConfig::setEnabledInProfileSettings(bool enabled)
{
    profileSettings()->setUseProfileSpecificDisplayBrightness(enabled);
}

QList<QPair<QString, QWidget *>> BrightnessControlConfig::buildUi()
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
    retlist.append(qMakePair(i18nc("Brightness level, label for the slider", "Level"), tempWidget));

    connect(m_slider, &QAbstractSlider::valueChanged, this, &BrightnessControlConfig::setChanged);

    return retlist;
}

}

#include "brightnesscontrolconfig.moc"

#include "moc_brightnesscontrolconfig.cpp"
