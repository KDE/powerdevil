/*
 * Copyright 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "turnoffkeyboardbacklightconfig.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>

#include <KConfig>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_FACTORY(TurnOffKeyboardBacklightConfigFactory, registerPlugin<PowerDevil::BundledActions::TurnOffKeyboardBacklightConfig>(); )

using namespace PowerDevil::BundledActions;

TurnOffKeyboardBacklightConfig::TurnOffKeyboardBacklightConfig(QObject *parent, const QVariantList &args)
    : ActionConfig(parent)
{
    Q_UNUSED(args)
}

TurnOffKeyboardBacklightConfig::~TurnOffKeyboardBacklightConfig() = default;

void TurnOffKeyboardBacklightConfig::save()
{
    configGroup().writeEntry("idleTime", m_spinBox->value() * 60 * 1000);
}

void TurnOffKeyboardBacklightConfig::load()
{
    configGroup().config()->reparseConfiguration();
    m_spinBox->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
}

QList< QPair< QString, QWidget* > > TurnOffKeyboardBacklightConfig::buildUi()
{
    m_spinBox = new QSpinBox;
    m_spinBox->setMaximumWidth(150);
    m_spinBox->setMinimum(1);
    m_spinBox->setMaximum(360);
    m_spinBox->setSuffix(i18n(" min"));

    QWidget *uiWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    uiWidget->setLayout(hlay);

    hlay->addWidget(m_spinBox);
    hlay->addStretch();

    QList< QPair< QString, QWidget* > > widgets;
    widgets.append(qMakePair< QString, QWidget* >(i18n("After"), uiWidget));

    connect(m_spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &TurnOffKeyboardBacklightConfig::setChanged);

    return widgets;
}

#include "turnoffkeyboardbacklightconfig.moc"
