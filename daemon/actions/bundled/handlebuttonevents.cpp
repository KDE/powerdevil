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
#include "handlebuttoneventsadaptor.h"

#include "suspendsession.h"

#include <powerdevilactionpool.h>

#include <KAction>
#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KIdleTime>

#include <Solid/Device>

#include "PowerDevilSettings.h"
#include "screensaver_interface.h"

#include <kglobalaccel.h>

namespace PowerDevil {
namespace BundledActions {

HandleButtonEvents::HandleButtonEvents(QObject* parent)
    : Action(parent)
    , m_lidAction(0)
    , m_powerButtonAction(0)
    , m_sleepButtonAction(1)
    , m_hibernateButtonAction(2)
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
    accel->setDefaultShortcut(globalAction, QList<QKeySequence>() << Qt::Key_Sleep);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToRam()));

    globalAction = actionCollection->addAction("Hibernate");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Hibernate"));
    accel->setDefaultShortcut(globalAction, QList<QKeySequence>() << Qt::Key_Hibernate);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(suspendToDisk()));

    globalAction = actionCollection->addAction("PowerOff");
    //globalAction->setText(i18nc("Global shortcut", "Power Off button"));
    accel->setDefaultShortcut(globalAction, QList<QKeySequence>() << Qt::Key_PowerOff);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(powerOffButtonTriggered()));
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
            // Check if the configuration makes it explicit or not
            processAction(m_lidAction, PowerDevilSettings::doNotInhibitOnLidClose());
            break;
        case BackendInterface::LidOpen:
            // In this case, let's send a wakeup event
            KIdleTime::instance()->simulateUserActivity();
            break;
        case BackendInterface::PowerButton:
            // This one is always explicit
            processAction(m_powerButtonAction, true);
            break;
        case BackendInterface::SleepButton:
            // This one is always explicit
            processAction(m_sleepButtonAction, true);
            break;
        case BackendInterface::HibernateButton:
            // This one is always explicit
            processAction(m_hibernateButtonAction, true);
            break;
        default:
            break;
    }
}

void HandleButtonEvents::processAction(uint action, bool isExplicit)
{
    // Basically, we simply trigger other actions :)
    switch ((SuspendSession::Mode)action) {
        case SuspendSession::TurnOffScreenMode:
            // Turn off screen
            triggerAction("DPMSControl", qVariantFromValue< QString >("TurnOff"), isExplicit);
            break;
        default:
            triggerAction("SuspendSession", qVariantFromValue< uint >(action), isExplicit);
            break;
    }
}

void HandleButtonEvents::triggerAction(const QString& action, const QVariant &type, bool isExplicit)
{
    PowerDevil::Action *helperAction = ActionPool::instance()->loadAction(action, KConfigGroup(), core());
    if (helperAction) {
        QVariantMap args;
        args["Type"] = type;
        args["Explicit"] = QVariant::fromValue(isExplicit);
        helperAction->trigger(args);
    }
}

void HandleButtonEvents::triggerImpl(const QVariantMap& args)
{
    // For now, let's just accept the phantomatic "32" button. It is also always explicit
    if (args["Button"].toInt() == 32) {
        if (args.contains("Type")) {
            triggerAction("SuspendSession", args["Type"], true);
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

int HandleButtonEvents::lidAction() const
{
    return m_lidAction;
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

}
}

#include "handlebuttonevents.moc"
