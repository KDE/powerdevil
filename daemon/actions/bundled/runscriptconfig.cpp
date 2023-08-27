/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "runscriptconfig.h"

#include <PowerDevilProfileSettings.h>

#include <QHBoxLayout>
#include <QSpinBox>

#include <KComboBox>
#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KUrlRequester>

K_PLUGIN_CLASS(PowerDevil::BundledActions::RunScriptConfig)

namespace RunScriptTrigger
{
enum ItemIndex { // keep in sync with m_comboBox->addItem() below
    OnProfileLoad = 0,
    OnProfileUnload,
    OnIdleTimeout,
};
}

namespace PowerDevil::BundledActions
{
RunScriptConfig::RunScriptConfig(QObject *parent)
    : ActionConfig(parent)
{
}

void RunScriptConfig::save()
{
    profileSettings()->setProfileLoadCommand(QString());
    profileSettings()->setProfileUnloadCommand(QString());
    profileSettings()->setIdleTimeoutCommand(QString());

    switch (m_comboBox->currentIndex()) {
    case RunScriptTrigger::OnProfileLoad:
        profileSettings()->setProfileLoadCommand(m_urlRequester->text());
        break;
    case RunScriptTrigger::OnProfileUnload:
        profileSettings()->setProfileUnloadCommand(m_urlRequester->text());
        break;
    case RunScriptTrigger::OnIdleTimeout:
        profileSettings()->setIdleTimeoutCommand(m_urlRequester->text());
        break;
    }
    profileSettings()->setRunScriptIdleTimeoutSec(m_idleTime->value() * 60);
}

void RunScriptConfig::load()
{
    if (!profileSettings()->profileLoadCommand().isEmpty()) {
        m_urlRequester->setText(profileSettings()->profileLoadCommand());
        m_comboBox->setCurrentIndex(RunScriptTrigger::OnProfileLoad);
    } else if (!profileSettings()->profileUnloadCommand().isEmpty()) {
        m_urlRequester->setText(profileSettings()->profileUnloadCommand());
        m_comboBox->setCurrentIndex(RunScriptTrigger::OnProfileUnload);
    } else if (!profileSettings()->idleTimeoutCommand().isEmpty()) {
        m_urlRequester->setText(profileSettings()->idleTimeoutCommand());
        m_comboBox->setCurrentIndex(RunScriptTrigger::OnIdleTimeout);
    }
    m_idleTime->setValue(profileSettings()->runScriptIdleTimeoutSec() / 60);
}

bool RunScriptConfig::enabledInProfileSettings() const
{
    return !profileSettings()->profileLoadCommand().isEmpty() || !profileSettings()->profileUnloadCommand().isEmpty()
        || !profileSettings()->idleTimeoutCommand().isEmpty();
}

void RunScriptConfig::setEnabledInProfileSettings(bool enabled)
{
    if (!enabled) {
        profileSettings()->setProfileLoadCommand(QString());
        profileSettings()->setProfileUnloadCommand(QString());
        profileSettings()->setIdleTimeoutCommand(QString());
        m_urlRequester->setText(QString());
    }
}

QList<QPair<QString, QWidget *>> RunScriptConfig::buildUi()
{
    QList<QPair<QString, QWidget *>> retlist;
    m_urlRequester = new KUrlRequester();
    m_urlRequester->setMode(KFile::File | KFile::LocalOnly | KFile::ExistingOnly);
    retlist.append(qMakePair(i18n("Script"), m_urlRequester));

    QWidget *tempWidget = new QWidget;
    QHBoxLayout *hlay = new QHBoxLayout;
    m_comboBox = new KComboBox;
    m_idleTime = new QSpinBox;
    m_idleTime->setMaximumWidth(150);
    m_idleTime->setMinimum(1);
    m_idleTime->setMaximum(360);
    m_idleTime->setValue(0);
    m_idleTime->setVisible(false);
    m_idleTime->setSuffix(i18n(" min"));
    m_comboBox->addItem(i18n("When entering power state")); // index 0 == RunScriptTrigger::OnProfileLoad
    m_comboBox->addItem(i18n("When exiting power state"));
    m_comboBox->addItem(i18n("After a period of inactivity"));
    connect(m_comboBox, &KComboBox::currentIndexChanged, this, [this](int index) {
        m_idleTime->setVisible(index == RunScriptTrigger::OnIdleTimeout);
    });

    hlay->addWidget(m_comboBox);
    hlay->addWidget(m_idleTime);
    hlay->addStretch();

    tempWidget->setLayout(hlay);

    retlist.append(qMakePair(i18n("Run script"), tempWidget));

    connect(m_urlRequester, &KUrlRequester::textChanged, this, &RunScriptConfig::setChanged);
    connect(m_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_idleTime, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    return retlist;
}
}

#include "runscriptconfig.moc"

#include "moc_runscriptconfig.cpp"
