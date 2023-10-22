/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "suspendsessionconfig.h"

#include <powerdevilenums.h>
#include <powerdevilpowermanagement.h>

#include <QHBoxLayout>
#include <QSpinBox>

#include <KComboBox>
#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QCheckBox>
#include <QIcon>

K_PLUGIN_CLASS(PowerDevil::BundledActions::SuspendSessionConfig)

namespace PowerDevil::BundledActions
{
SuspendSessionConfig::SuspendSessionConfig(QObject *parent)
    : ActionConfig(parent)
    , m_suspendThenHibernateEnabled(nullptr)
{
}

void SuspendSessionConfig::save()
{
    configGroup().writeEntry<uint>("suspendType", m_comboBox->itemData(m_comboBox->currentIndex()).toUInt());
    configGroup().writeEntry("idleTime", m_idleTime->value() * 60 * 1000);
    configGroup().writeEntry<bool>("suspendThenHibernate", m_suspendThenHibernateEnabled != nullptr && m_suspendThenHibernateEnabled->isChecked());

    configGroup().sync();
}

void SuspendSessionConfig::load()
{
    configGroup().config()->reparseConfiguration();

    PowerDevil::PowerButtonAction defaultAction = (PowerManagement::instance()->canSuspend() //
                                                       ? PowerDevil::PowerButtonAction::SuspendToRam
                                                       : PowerDevil::PowerButtonAction::Shutdown);
    uint suspendType = configGroup().readEntry<uint>("suspendType", static_cast<uint>(defaultAction));
    m_comboBox->setCurrentIndex(m_comboBox->findData(suspendType));
    m_idleTime->setValue((configGroup().readEntry<int>("idleTime", 600000) / 60) / 1000);
    if (m_suspendThenHibernateEnabled != nullptr) {
        m_suspendThenHibernateEnabled->setChecked(configGroup().readEntry<bool>("suspendThenHibernate", false));
    }
}

QList<QPair<QString, QWidget *>> SuspendSessionConfig::buildUi()
{
    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new QSpinBox;
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setMinimum(1);
    m_idleTime->setMaximum(360);
    m_idleTime->setValue(0);
    m_idleTime->setSuffix(i18n(" min"));
    m_idleTime->setPrefix(i18n("after "));

    if (PowerManagement::instance()->canSuspend()) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend"),
                            i18nc("Suspend to RAM", "Sleep"),
                            static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToRam));
    }
    if (PowerManagement::instance()->canHibernate()) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToDisk));
    }
    if (PowerManagement::instance()->canHybridSuspend()) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend-hybrid"), i18n("Hybrid sleep"), static_cast<uint>(PowerDevil::PowerButtonAction::SuspendHybrid));
    }
    m_comboBox->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), static_cast<uint>(PowerDevil::PowerButtonAction::Shutdown));
    m_comboBox->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), static_cast<uint>(PowerDevil::PowerButtonAction::LockScreen));

    hlay->addWidget(m_comboBox);
    hlay->addWidget(m_idleTime);
    hlay->addStretch();

    tempWidget->setLayout(hlay);

    QList<QPair<QString, QWidget *>> retlist;
    retlist.append(qMakePair(i18n("Automatically"), tempWidget));

    connect(m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_idleTime, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    if (PowerManagement::instance()->canSuspendThenHibernate()) {
        m_suspendThenHibernateEnabled = new QCheckBox(i18n("While asleep, hibernate after a period of inactivity"));
        connect(m_suspendThenHibernateEnabled, &QCheckBox::stateChanged, this, &SuspendSessionConfig::setChanged);
        retlist.append(qMakePair<QString, QWidget *>(QLatin1String("NONE"), m_suspendThenHibernateEnabled));
        int comboBoxMaxWidth = 300;
        comboBoxMaxWidth = qMax(comboBoxMaxWidth, m_suspendThenHibernateEnabled->sizeHint().width());
        m_suspendThenHibernateEnabled->setMinimumWidth(300);
        m_suspendThenHibernateEnabled->setMaximumWidth(comboBoxMaxWidth);
    }

    return retlist;
}

}

#include "suspendsessionconfig.moc"

#include "moc_suspendsessionconfig.cpp"
