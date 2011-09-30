/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#include "suspendsession.h"

#include <powerdevilactionpool.h>

#include <KConfigGroup>
#include <KIdleTime>

#include "screensaver_interface.h"

namespace PowerDevil {
namespace BundledActions {

HandleButtonEvents::HandleButtonEvents(QObject* parent)
    : Action(parent)
    , m_lidAction(0)
    , m_powerButtonAction(0)
{
    // We enforce no policies here - after all, we just call other actions - which have their policies.
    setRequiredPolicies(PowerDevil::PolicyAgent::None);
    connect(backend(), SIGNAL(buttonPressed(PowerDevil::BackendInterface::ButtonType)),
            this, SLOT(onButtonPressed(PowerDevil::BackendInterface::ButtonType)));
}

HandleButtonEvents::~HandleButtonEvents()
{

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
            processAction(m_lidAction);
            break;
        case BackendInterface::LidOpen:
            // In this case, let's send a wakeup event
            KIdleTime::instance()->simulateUserActivity();
            break;
        case BackendInterface::PowerButton:
            processAction(m_powerButtonAction);
            break;
        default:
            break;
    }
}

void HandleButtonEvents::processAction(uint action)
{
    // Basically, we simply trigger other actions :)
    switch ((SuspendSession::Mode)action) {
        case SuspendSession::TurnOffScreenMode:
            // Turn off screen
            triggerAction("DPMSControl", qVariantFromValue< QString >("TurnOff"));
            break;
        default:
            triggerAction("SuspendSession", qVariantFromValue< uint >(action));
            break;
    }
}

void HandleButtonEvents::triggerAction(const QString& action, const QVariant &type)
{
    PowerDevil::Action *helperAction = ActionPool::instance()->loadAction(action, KConfigGroup(), core());
    if (helperAction) {
        QVariantMap args;
        args["Type"] = type;
        helperAction->trigger(args);
    }
}

void HandleButtonEvents::triggerImpl(const QVariantMap& args)
{
    // For now, let's just accept the phantomatic "32" button.
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
    m_powerButtonAction = config.readEntry<uint>("powerButtonAction", 0);

    return true;
}

}
}

#include "handlebuttonevents.moc"
