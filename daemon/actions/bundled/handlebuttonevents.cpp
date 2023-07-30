/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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

#include "handlebuttonevents.h"
#include "handlebuttoneventsadaptor.h"

#include <powerdevil_debug.h>
#include <powerdevilactionpool.h>
#include <powerdevilenums.h>

#include <QAction>

#include <KActionCollection>
#include <KConfigGroup>
#include <KIdleTime>
#include <KLocalizedString>

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Output>

#include <KGlobalAccel>
#include <Kirigami/TabletModeWatcher>

namespace PowerDevil::BundledActions
{
HandleButtonEvents::HandleButtonEvents(QObject *parent)
    : Action(parent)
    , m_screenConfiguration(nullptr)
{
    new HandleButtonEventsAdaptor(this);
    // We enforce no policies here - after all, we just call other actions - which have their policies.
    setRequiredPolicies(PowerDevil::PolicyAgent::None);
    connect(backend(), &BackendInterface::buttonPressed, this, &HandleButtonEvents::onButtonPressed);

    KActionCollection *actionCollection = new KActionCollection(this);
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));

    KGlobalAccel *accel = KGlobalAccel::self();

    QAction *globalAction = actionCollection->addAction("Sleep");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Suspend"));
    accel->setGlobalShortcut(globalAction, Qt::Key_Sleep);
    connect(globalAction, &QAction::triggered, this, &HandleButtonEvents::suspendToRam);

    globalAction = actionCollection->addAction("Hibernate");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Hibernate"));
    accel->setGlobalShortcut(globalAction, Qt::Key_Hibernate);
    connect(globalAction, &QAction::triggered, this, &HandleButtonEvents::suspendToDisk);

    globalAction = actionCollection->addAction("PowerOff");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Power Off"));
    auto powerButtonMode = [globalAction](bool isTablet) {
        if (!isTablet) {
            KGlobalAccel::self()->setGlobalShortcut(globalAction, Qt::Key_PowerOff);
        } else {
            KGlobalAccel::self()->setGlobalShortcut(globalAction, QList<QKeySequence>());
        }
    };
    auto interface = Kirigami::TabletModeWatcher::self();
    connect(interface, &Kirigami::TabletModeWatcher::tabletModeChanged, globalAction, powerButtonMode);
    powerButtonMode(interface->isTabletMode());
    connect(globalAction, &QAction::triggered, this, &HandleButtonEvents::powerOffButtonTriggered);

    globalAction = actionCollection->addAction("PowerDown");
    globalAction->setText(i18nc("@action:inmenu Global shortcut, used for long presses of the power button", "Power Down"));
    accel->setGlobalShortcut(globalAction, Qt::Key_PowerDown);
    connect(globalAction, &QAction::triggered, this, &HandleButtonEvents::powerDownButtonTriggered);

    connect(new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoEDID),
            &KScreen::ConfigOperation::finished,
            this,
            [this](KScreen::ConfigOperation *op) {
                m_screenConfiguration = qobject_cast<KScreen::GetConfigOperation *>(op)->config();
                checkOutputs();

                KScreen::ConfigMonitor::instance()->addConfig(m_screenConfiguration);
                connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged, this, &HandleButtonEvents::checkOutputs);
            });

    if (!backend()->isLidClosed()) {
        m_oldKeyboardBrightness = backend()->keyboardBrightness();
    }
    connect(backend(), &PowerDevil::BackendInterface::keyboardBrightnessChanged, this, [this](const BrightnessLogic::BrightnessInfo &brightnessInfo) {
        // By the time the lid close is processed, the backend brightness will already be updated.
        // That's why we track the brightness here as long as the lid is open.
        m_oldKeyboardBrightness = brightnessInfo.value;
    });
}

HandleButtonEvents::~HandleButtonEvents()
{
}

bool HandleButtonEvents::isSupported()
{
    // we handles keyboard shortcuts in our button handling, users always have a keyboard
    return true;
}

void HandleButtonEvents::onProfileUnload()
{
    m_lidAction = 0;
    m_powerButtonAction = 0;
}

void HandleButtonEvents::onWakeupFromIdle()
{
    //
}

void HandleButtonEvents::onIdleTimeout(int msec)
{
    Q_UNUSED(msec)
}

void HandleButtonEvents::onProfileLoad(const QString & /*previousProfile*/, const QString & /*newProfile*/)
{
    //
}

void HandleButtonEvents::onButtonPressed(BackendInterface::ButtonType type)
{
    switch (type) {
    case BackendInterface::LidClose:
        if (m_oldKeyboardBrightness.has_value()) {
            backend()->setKeyboardBrightness(0);
        }

        if (!triggersLidAction()) {
            qCWarning(POWERDEVIL) << "Lid action was suppressed because an external monitor is present";
            return;
        }

        processAction(m_lidAction);
        break;
    case BackendInterface::LidOpen:
        // When we restore the keyboard brightness before waking up, we shouldn't conflict
        // with dimdisplay or dpms also messing with the keyboard.
        if (m_oldKeyboardBrightness.has_value() && m_oldKeyboardBrightness > 0) {
            backend()->setKeyboardBrightness(m_oldKeyboardBrightness.value());
        }

        // In this case, let's send a wakeup event
        KIdleTime::instance()->simulateUserActivity();
        break;
    case BackendInterface::PowerButton:
        processAction(m_powerButtonAction);
        break;
    case BackendInterface::PowerDownButton:
        processAction(m_powerDownButtonAction);
        break;
    case BackendInterface::SleepButton:
        processAction(m_sleepButtonAction);
        break;
    case BackendInterface::HibernateButton:
        processAction(m_hibernateButtonAction);
        break;
    default:
        break;
    }
}

void HandleButtonEvents::processAction(uint action)
{
    // Basically, we simply trigger other actions :)
    switch (static_cast<PowerDevil::PowerButtonAction>(action)) {
    case PowerDevil::PowerButtonAction::TurnOffScreen:
        // Turn off screen
        triggerAction("DPMSControl", QStringLiteral("TurnOff"));
        break;
    case PowerDevil::PowerButtonAction::ToggleScreenOnOff:
        // Toggle screen on/off
        triggerAction("DPMSControl", QStringLiteral("ToggleOnOff"));
        break;
    default:
        triggerAction("SuspendSession", action);
        break;
    }
}

void HandleButtonEvents::triggerAction(const QString &action, const QVariant &type)
{
    PowerDevil::Action *helperAction = ActionPool::instance()->loadAction(action, KConfigGroup(), core());
    if (helperAction) {
        helperAction->trigger({
            {QStringLiteral("Type"), type},
            {QStringLiteral("Explicit"), true},
        });
    }
}

void HandleButtonEvents::triggerImpl(const QVariantMap &args)
{
    // For now, let's just accept the phantomatic "32" button. It is also always explicit
    if (args["Button"].toInt() == 32) {
        if (args.contains("Type")) {
            triggerAction("SuspendSession", args["Type"]);
        }
    }
}

bool HandleButtonEvents::loadAction(const KConfigGroup &config)
{
    // Read configs
    m_lidAction = config.readEntry<uint>("lidAction", 0);
    m_triggerLidActionWhenExternalMonitorPresent = config.readEntry<bool>("triggerLidActionWhenExternalMonitorPresent", false);
    m_powerButtonAction = config.readEntry<uint>("powerButtonAction", 0);
    m_powerDownButtonAction = config.readEntry<uint>("powerDownAction", 0);

    checkOutputs();

    return true;
}

int HandleButtonEvents::lidAction() const
{
    return m_lidAction;
}

bool HandleButtonEvents::triggersLidAction() const
{
    return m_triggerLidActionWhenExternalMonitorPresent || !m_externalMonitorPresent;
}

void HandleButtonEvents::powerOffButtonTriggered()
{
    onButtonPressed(BackendInterface::PowerButton);
}

void HandleButtonEvents::powerDownButtonTriggered()
{
    onButtonPressed(BackendInterface::PowerDownButton);
}

void HandleButtonEvents::suspendToDisk()
{
    onButtonPressed(BackendInterface::HibernateButton);
}

void HandleButtonEvents::suspendToRam()
{
    onButtonPressed(BackendInterface::SleepButton);
}

void HandleButtonEvents::checkOutputs()
{
    if (!m_screenConfiguration) {
        qCWarning(POWERDEVIL) << "Handle button events action could not check for screen configuration";
        return;
    }

    const bool old_triggersLidAction = triggersLidAction();

    bool hasExternalMonitor = false;

    for (const KScreen::OutputPtr &output : m_screenConfiguration->outputs()) {
        if (output->isConnected() && output->isEnabled() && output->type() != KScreen::Output::Panel && output->type() != KScreen::Output::Unknown) {
            hasExternalMonitor = true;
            break;
        }
    }

    m_externalMonitorPresent = hasExternalMonitor;

    if (old_triggersLidAction != triggersLidAction()) {
        Q_EMIT triggersLidActionChanged(triggersLidAction());

        // when the lid is closed but we don't suspend because of an external monitor but we then
        // unplug said monitor, re-trigger the lid action (Bug 379265)
        if (triggersLidAction() && backend()->isLidClosed()) {
            qCDebug(POWERDEVIL) << "External monitor that kept us from suspending is gone and lid is closed, re-triggering lid action";
            onButtonPressed(BackendInterface::LidClose);
        }
    }
}

}

#include "moc_handlebuttonevents.cpp"
