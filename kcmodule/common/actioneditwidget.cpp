/***************************************************************************
 *   Copyright (C) 2008-2011 by Dario Freddi <drf@kde.org>                 *
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


#include "actioneditwidget.h"

#include <powerdevilaction.h>

#include <powerdevil_debug.h>

#include <QCheckBox>
#include <QVBoxLayout>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QDebug>

#include <KConfigGroup>
#include <KServiceTypeTrader>
#include <KLocalizedString>

#include "ui_actioneditwidget.h"
#include "powerdevilpowermanagement.h"
#include "upower_interface.h"
#include "powerdevilenums.h"

namespace {
    int msecToMinutes(int msec) {
        return msec / 60 / 1000;
    }
    int minutesToMsec(int minutes) {
        return minutes * 60 * 1000;
    }
    int secToMinutes(int sec) {
        return sec / 60;
    }
    int minutesToSec(int minutes) {
        return minutes * 60;
    }
}

ActionEditWidget::ActionEditWidget(const QString &configName, QWidget *parent)
    : QWidget(parent)
    , m_configName(configName)
    , m_profilesConfig(configName)
    , ui(std::make_unique<Ui::ActionEditWidget>())
{
    ui->setupUi(this);
    setObjectName(configName);

    auto *PM = PowerDevil::PowerManagement::instance();
    {
        using Enum = PowerDevilEnums::PowerButtonMode;


        if (PM->canSuspend()) {
            ui->kcfg_SuspendType->addItem(QIcon::fromTheme("system-suspend"), i18nc("Suspend to RAM", "Sleep"), Enum::ToRamMode);
        }
        if (PM->canHibernate()) {
            ui->kcfg_SuspendType->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), Enum::ToDiskMode);
        }
        if (PM->canHybridSuspend()) {
            ui->kcfg_SuspendType->addItem(QIcon::fromTheme("system-suspend-hybrid"), i18n("Hybrid sleep"), Enum::SuspendHybridMode);
        }
        ui->kcfg_SuspendType->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), Enum::ShutdownMode);
        ui->kcfg_SuspendType->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), Enum::LockScreenMode);
    }

    {
        using Enum = PowerDevilEnums::RunScriptMode;
        ui->kcfg_RunScriptPhase->addItem(QIcon(), i18n("On profile load"), Enum::OnProfileLoad);
        ui->kcfg_RunScriptPhase->addItem(QIcon(), i18n("On profile unload"), Enum::OnProfileUnload);
        ui->kcfg_RunScriptPhase->addItem(QIcon(), i18n("After"), Enum::After);
    }

    for (auto combo : {ui->kcfg_BluetoothPowerSaving, ui->kcfg_MobileBroadbandPowerSaving, ui->kcfg_WifiPowerSaving}) {
        using Enum = PowerDevilEnums::WirelessMode;
        combo->addItem(QIcon(), i18n("Leave unchanged"), Enum::NoAction);
        combo->addItem(QIcon(), i18n("Turn off"), Enum::TurnOff);
        combo->addItem(QIcon(), i18n("Turn on"), Enum::TurnOn);
    }

    for (QComboBox *box : {ui->kcfg_PowerButtonAction, ui->kcfg_LidAction}) {
        using Enum = PowerDevilEnums::PowerButtonMode;
        box->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), Enum::NoneMode);

        if (PowerDevil::PowerManagement::instance()->canSuspend()) {
            box->addItem(QIcon::fromTheme("system-suspend"), i18nc("Suspend to RAM", "Sleep"), Enum::ToRamMode);
        }

        if (PowerDevil::PowerManagement::instance()->canHibernate()) {
            box->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), Enum::ToDiskMode);
        }

        if (PowerDevil::PowerManagement::instance()->canHybridSuspend()) {
            box->addItem(QIcon::fromTheme("system-suspend-hybrid"), i18n("Hybrid sleep"), Enum::SuspendHybridMode);
        }


        box->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), Enum::ShutdownMode);
        box->addItem(QIcon::fromTheme("system-lock-screen"), i18n("Lock screen"), Enum::LockScreenMode);

        if (box != ui->kcfg_LidAction) {
            box->addItem(QIcon::fromTheme("system-log-out"), i18n("Prompt log out dialog"), Enum::LogoutDialogMode);
        }
        box->addItem(QIcon::fromTheme("preferences-desktop-screensaver"), i18n("Turn off screen"), Enum::TurnOffScreenMode);
    }

    // Hide elements based on service avaliability
    auto upowerInterface = new OrgFreedesktopUPowerInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", QDBusConnection::systemBus(), this);

    if (!upowerInterface->lidIsPresent()) {
        ui->kcfg_LidAction->hide();
        ui->laptopLidLabel->hide();
        ui->kcfg_TriggerLidActionWhenExternalMonitorPresent->hide();
    }

    if (!PM->canSuspendThenHibernate()) {
        ui->kcfg_SuspendThenHibernate->hide();
    }

    connect(ui->suspendSessionIdleTimeMsec, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] (int value) {
        m_profilesConfig.setSuspendSessionIdleTimeMsec(minutesToMsec(value));
        triggerStateRequest();
    });

    connect(ui->dimDisplayIdleTimeMsec, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] (int value) {
        m_profilesConfig.setDimDisplayIdleTimeMsec(minutesToMsec(value));
        triggerStateRequest();
    });

    connect(ui->dpmsDisplayIdleTimeSec,QOverload<int>::of(&QSpinBox::valueChanged), this, [this] (int value) {
        m_profilesConfig.setDPMSidleTimeSec(minutesToSec(value));
        triggerStateRequest();
    });

    connect(ui->kernelPowerProfileCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_profilesConfig.setKernelPowerProfile(ui->kernelPowerProfileCombo->currentData().toString());
        triggerStateRequest();
    });

    initializeServices();
}

ActionEditWidget::~ActionEditWidget() = default;

void ActionEditWidget::triggerStateRequest()
{
    if (m_profilesConfig.isDefaults()) {
        requestDefaultState();
    } else {
        requestChangeState();
    }
}

void ActionEditWidget::save()
{
    m_profilesConfig.save();
}

void ActionEditWidget::load()
{
    ui->suspendSessionIdleTimeMsec->setValue(msecToMinutes(m_profilesConfig.suspendSessionIdleTimeMsec()));

    ui->dimDisplayIdleTimeMsec->setValue(msecToMinutes(
        m_profilesConfig.dimDisplayIdleTimeMsec()));

    qDebug() << "Loading DimDisplay for profile" << m_configName
        << "with value" << m_profilesConfig.dimDisplayIdleTimeMsec()
        << "And after transforming to minutes:"
        << msecToMinutes(m_profilesConfig.dimDisplayIdleTimeMsec());

    ui->dpmsDisplayIdleTimeSec->setValue(secToMinutes(m_profilesConfig.dPMSidleTimeSec()));

    ui->screenBrightness->setEnabled(ui->kcfg_ManageBrightnessControl->isChecked());
    ui->keyboardBrightness->setEnabled(ui->kcfg_ManageKeyboardBrightness->isChecked());
    ui->dimDisplayWdg->setEnabled(ui->kcfg_ManageDimDisplay->isChecked());
    ui->suspendSessionWdg->setEnabled(ui->kcfg_ManageSuspendSession->isChecked());
    ui->buttonEventHandlingWdg->setEnabled(ui->kcfg_ManageButtonEvents->isChecked());
    ui->runScriptWdg->setEnabled(ui->kcfg_ManageRunScript->isChecked());
    ui->wirelessWdg->setEnabled(ui->kcfg_ManageWirelessPowerSaving->isChecked());
    ui->kernelPowerProfile->setEnabled(ui->kcfg_ManageKernelPowerProfile->isChecked());
    ui->dpmsWdg->setEnabled(ui->kcfg_ManageDPMS->isChecked());

}

QString ActionEditWidget::configName() const
{
    return m_configName;
}

void ActionEditWidget::initializeServices()
{

    qDebug() << " -=-=-= ";
    // Maps power devil action names to widgets so we can easily disable those when
    // the action is not supported.
    auto widgets = std::map<QString, std::pair<QCheckBox*, QWidget*>> {
        {QStringLiteral("PowerProfile"), {ui->kcfg_ManageKernelPowerProfile, ui->kernelPowerProfile}},
        {QStringLiteral("BrightnessControl"), {ui->kcfg_ManageBrightnessControl, ui->screenBrightness}},
        {QStringLiteral("KeyboardBrightnessControl"), {ui->kcfg_ManageKeyboardBrightness, ui->keyboardBrightness}},
        {QStringLiteral("DimDisplay"), {ui->kcfg_ManageDimDisplay, ui->dimDisplayWdg}},
        {QStringLiteral("HandleButtonEvents"), {ui->kcfg_ManageButtonEvents, ui->buttonEventHandlingWdg}},
        {QStringLiteral("DPMSControl"), {ui->kcfg_ManageDPMS, ui->dpmsWdg}},
        {QStringLiteral("WirelessPowerSaving"), {ui->kcfg_ManageWirelessPowerSaving, ui->wirelessWdg}},
        {QStringLiteral("SuspendSession"), {ui->kcfg_ManageSuspendSession, ui->suspendSessionWdg}},
        {QStringLiteral("RunScript"), {ui->kcfg_ManageRunScript, ui->runScriptWdg}},
    };


    // We need to query DBUs for the actions, those where usually set by the plugin but we removed plugins from here. Here's a
    // handwritten list.
    for (const auto& actionId : {
            "RunScript", "SuspendSession", "PowerProfile",
            "BrightnessControl", "KeyboardBrightnessControl", "DPMSControl",
            "HandleButtonEvents", "WirelessPowerSaving", "DimDisplay"})
    {
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("isActionSupported"));

        call.setArguments({actionId});

        QDBusPendingReply< bool > reply = QDBusConnection::sessionBus().asyncCall(call);
        reply.waitForFinished();

        if (!reply.isValid()) {
            qDebug() << "There was a problem in contacting DBus!! Assuming the action is ok.";
            continue;
        }

        if (!reply.value()) {
            widgets[actionId].first->setVisible(false);
            widgets[actionId].second->setVisible(false);
        }
    }

    /* Loads the values for the Kernel Power Profile Modes */
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                      QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                      QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                      QStringLiteral("profileChoices"));

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();

        ui->kernelPowerProfileCombo->clear();
        ui->kernelPowerProfileCombo->addItem(i18n("Leave unchanged"));

        if (reply.isError()) {
            qWarning() << "Failed to query platform profile choices" << reply.error().message();
            return;
        }

        const QHash<QString, QString> profileNames = {
            {QStringLiteral("power-saver"), i18n("Power Save")},
            {QStringLiteral("balanced"), i18n("Balanced")},
            {QStringLiteral("performance"), i18n("Performance")},
        };

        const QStringList choices = reply.value();
        for (const QString &choice : choices) {
            const QString profileName = profileNames.value(choice, choice);
            const QString profileKey = choice;
            ui->kernelPowerProfileCombo->addItem(profileName, profileKey);
            qDebug() << "Loading" << choice << "profile";
        }

        qDebug() << "Trying to find the following profile" <<
m_profilesConfig.kernelPowerProfile();
        qDebug() << "If that's not loaded, please poke me.";

        const int idx = ui->kernelPowerProfileCombo->
            findData(m_profilesConfig.kernelPowerProfile());

        if (idx == -1) {
            qDebug() << "Error finding"
            << m_profilesConfig.kernelPowerProfile()
            << "on the choice combobox defaulting to nothing";
        }

        ui->kernelPowerProfileCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    });
}
