/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dpmsconfig.h"

#include <QSpinBox>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_CLASS(PowerDevilDPMSActionConfig)

PowerDevilDPMSActionConfig::PowerDevilDPMSActionConfig(QObject *parent)
    : ActionConfig(parent)
{
}

void PowerDevilDPMSActionConfig::save()
{
    configGroup().writeEntry("idleTime", m_spinBox->value() * 60);
    configGroup().writeEntry("idleTimeoutWhenLocked", m_spinIdleTimeoutLocked->value());

    configGroup().sync();
}

void PowerDevilDPMSActionConfig::load()
{
    configGroup().config()->reparseConfiguration();
    m_spinBox->setValue(configGroup().readEntry<int>("idleTime", 600) / 60);
    m_spinIdleTimeoutLocked->setValue(configGroup().readEntry<int>("idleTimeoutWhenLocked", 60));
}

QList<QPair<QString, QWidget *>> PowerDevilDPMSActionConfig::buildUi()
{
    QList<QPair<QString, QWidget *>> retlist;

    m_spinBox = new QSpinBox;
    m_spinBox->setMaximumWidth(150);
    m_spinBox->setMinimum(1);
    m_spinBox->setMaximum(360);
    m_spinBox->setSuffix(i18n(" min"));
    retlist.append(qMakePair<QString, QWidget *>(i18n("Switch off after"), m_spinBox));

    connect(m_spinBox, &QSpinBox::valueChanged, this, &PowerDevilDPMSActionConfig::setChanged);

    m_spinIdleTimeoutLocked = new QSpinBox;
    m_spinIdleTimeoutLocked->setMaximumWidth(150);
    m_spinIdleTimeoutLocked->setMinimum(0);
    m_spinIdleTimeoutLocked->setMaximum(360);
    m_spinIdleTimeoutLocked->setSuffix(i18n(" sec"));
    retlist.append(qMakePair<QString, QWidget *>(i18n("When screen is locked, switch off after"), m_spinIdleTimeoutLocked));

    connect(m_spinIdleTimeoutLocked, &QSpinBox::valueChanged, this, &PowerDevilDPMSActionConfig::setChanged);

    return retlist;
}

#include "dpmsconfig.moc"

#include "moc_dpmsconfig.cpp"
