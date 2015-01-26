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

#include "suspendsession.h"

#include <powerdevilactionpool.h>
#include <powerdevil_debug.h>

#include <QAction>

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KIdleTime>

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Output>

#include "screensaver_interface.h"

#include <kglobalaccel.h>

namespace PowerDevil {
namespace BundledActions {

HandleButtonEvents::HandleButtonEvents(QObject *parent)
    : Action(parent)
    , m_screenConfiguration(nullptr)
{
    new HandleButtonEventsAdaptor(this);
    // We enforce no policies here - after all, we just call other actions - which have their policies.
    setRequiredPolicies(PowerDevil::PolicyAgent::None);
    connect(backend(), SIGNAL(buttonPressed(PowerDevil::BackendInterface::ButtonType)),
            this, SLOT(onButtonPressed(PowerDevil::BackendInterface::ButtonType)));

    KActionCollection* actionCollection = new KActionCollection( this );
    KGlobalAccel *accel = KGlobalAccel::self();

    QAction *globalAction = actionCollection->addAction("Sleep");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Sleep"));
    accel->setGlobalShortcut(globalAction, Qt::Key_Sleep);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToRam()));

    globalAction = actionCollection->addAction("Hibernate");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Hibernate"));
    accel->setGlobalShortcut(globalAction, Qt::Key_Hibernate);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToDisk()));

    globalAction = actionCollection->addAction("PowerOff");
    //globalAction->setText(i18nc("Global shortcut", "Power Off button"));
    accel->setGlobalShortcut(globalAction, Qt::Key_PowerOff);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(powerOffButtonTriggered()));

    connect(new KScreen::GetConfigOperation(KScreen::GetConfigOperation::NoEDID), &KScreen::ConfigOperation::finished,
            this, [this](KScreen::ConfigOperation *op) {
                m_screenConfiguration = qobject_cast<KScreen::GetConfigOperation *>(op)->config();
                checkOutputs();

                KScreen::ConfigMonitor::instance()->addConfig(m_screenConfiguration);
                connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged, this, &HandleButtonEvents::checkOutputs);
    });
}

HandleButtonEvents::~HandleButtonEvents()
{

}

bool HandleButtonEvents::isSupported()
{
    return backend()->isLidPresent();
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

void HandleButtonEvents::onProfileLoad()
{
    //
}

void HandleButtonEvents::onButtonPressed(BackendInterface::ButtonType type)
{
    switch (type) {
        case BackendInterface::LidClose:
            if (!triggersLidAction()) {
                qCWarning(POWERDEVIL) << "Lid action was suppressed because an external monitor is present";
                return;
            }

            processAction(m_lidAction);
            break;
        case BackendInterface::LidOpen:
            // In this case, let's send a wakeup event
            KIdleTime::instance()->simulateUserActivity();
            break;
        case BackendInterface::PowerButton:
            processAction(m_powerButtonAction);
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
    switch (static_cast<SuspendSession::Mode>(action)) {
        case SuspendSession::TurnOffScreenMode:
            // Turn off screen
            triggerAction("DPMSControl", QStringLiteral("TurnOff"));
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
            {QStringLiteral("Explicit"), true}
        });
    }
}

void HandleButtonEvents::triggerImpl(const QVariantMap& args)
{
    // For now, let's just accept the phantomatic "32" button. It is also always explicit
    if (args["Button"].toInt() == 32) {
        if (args.contains("Type")) {
            triggerAction("SuspendSession", args["Type"]);
        }
    }
}

bool HandleButtonEvents::loadAction(const KConfigGroup& config)
{
    // Read configs
    m_lidAction = config.readEntry<uint>("lidAction", 0);
    m_triggerLidActionWhenExternalMonitorPresent = config.readEntry<bool>("triggerLidActionWhenExternalMonitorPresent", false);
    m_powerButtonAction = config.readEntry<uint>("powerButtonAction", 0);

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

    for(const KScreen::OutputPtr &output : m_screenConfiguration->outputs()) {
        if (output->isConnected() && output->isEnabled() && output->type() != KScreen::Output::Panel) {
            hasExternalMonitor = true;
            break;
        }
    }

    m_externalMonitorPresent = hasExternalMonitor;

    if (old_triggersLidAction != triggersLidAction()) {
        emit triggersLidActionChanged(triggersLidAction());
    }
}

}
}
