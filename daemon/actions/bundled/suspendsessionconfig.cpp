/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "suspendsessionconfig.h"

#include <PowerDevilProfileSettings.h>
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
    , m_sleepModeCombo(nullptr)
{
}

void SuspendSessionConfig::save()
{
    profileSettings()->setAutoSuspendAction(m_comboBox->itemData(m_comboBox->currentIndex()).toUInt());
    profileSettings()->setAutoSuspendIdleTimeoutSec(m_idleTime->value() * 60);
    if (m_sleepModeCombo != nullptr) {
        profileSettings()->setSleepMode(m_sleepModeCombo->itemData(m_sleepModeCombo->currentIndex()).toUInt());
    }
}

void SuspendSessionConfig::load()
{
    m_comboBox->setCurrentIndex(m_comboBox->findData(profileSettings()->autoSuspendAction()));
    m_idleTime->setValue(profileSettings()->autoSuspendIdleTimeoutSec() / 60);
    if (m_sleepModeCombo != nullptr) {
        m_sleepModeCombo->setCurrentIndex(m_sleepModeCombo->findData(profileSettings()->sleepMode()));
    }
}

bool SuspendSessionConfig::enabledInProfileSettings() const
{
    return profileSettings()->autoSuspendAction() != qToUnderlying(PowerDevil::PowerButtonAction::NoAction);
}

void SuspendSessionConfig::setEnabledInProfileSettings(bool enabled)
{
    if (!enabled) {
        profileSettings()->setAutoSuspendAction(qToUnderlying(PowerDevil::PowerButtonAction::NoAction));
        m_comboBox->setCurrentIndex(0); // NoAction
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

    m_comboBox->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), static_cast<uint>(PowerDevil::PowerButtonAction::NoAction));

    bool canSleep =
        PowerManagement::instance()->canSuspend() || PowerManagement::instance()->canHybridSuspend() || PowerManagement::instance()->canSuspendThenHibernate();

    if (canSleep) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend"),
                            i18nc("Suspend to RAM", "Sleep"),
                            static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToRam));
    }
    if (PowerManagement::instance()->canHibernate()) {
        m_comboBox->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), static_cast<uint>(PowerDevil::PowerButtonAction::SuspendToDisk));
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
    connect(m_comboBox, &QComboBox::currentIndexChanged, [this](int index) {
        m_idleTime->setEnabled(index != 0); // i.e. disable when NoAction is selected
    });

    if (canSleep) {
        m_sleepModeCombo = new KComboBox;

        // TODO: Split item labels into label/description,
        //       synchronize texts with plasma-workspace/runners/powerdevil/PowerDevilRunner.cpp
        if (PowerManagement::instance()->canSuspend()) {
            m_sleepModeCombo->addItem(i18n("Standby (Save session to memory)"), static_cast<uint>(PowerDevil::SleepMode::SuspendToRam));
        }
        if (PowerManagement::instance()->canHybridSuspend()) {
            m_sleepModeCombo->addItem(i18n("Hybrid sleep (Save session to both memory and disk)"), static_cast<uint>(PowerDevil::SleepMode::HybridSuspend));
        }
        if (PowerManagement::instance()->canSuspendThenHibernate()) {
            m_sleepModeCombo->addItem(i18n("Standby, then hibernate after a period of inactivity"),
                                      static_cast<uint>(PowerDevil::SleepMode::SuspendThenHibernate));
        }

        if (m_sleepModeCombo->count() == 1) {
            retlist.append(qMakePair("NONE", m_sleepModeCombo));
            m_sleepModeCombo->setVisible(false);
        } else {
            retlist.append(
                qMakePair(i18nc("@label:combobox Sleep mode selection - suspend to memory, disk or both", "When sleeping, enter"), m_sleepModeCombo));

            m_sleepModeCombo->setMinimumWidth(300);
            m_sleepModeCombo->setMaximumWidth(qMax(300, m_sleepModeCombo->sizeHint().width()));
        }

        connect(m_sleepModeCombo, &QComboBox::currentIndexChanged, this, &SuspendSessionConfig::setChanged);
    }

    return retlist;
}

}

#include "suspendsessionconfig.moc"

#include "moc_suspendsessionconfig.cpp"
