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

#include "powerdevilaction.h"

#include "powerdevilcore.h"

#include <KDebug>

namespace PowerDevil
{

class Action::Private
{
public:
    Private() {}
    ~Private() {}

    PowerDevil::Core *core;

    QList< int > registeredIdleTimeouts;
    PowerDevil::PolicyAgent::RequiredPolicies requiredPolicies;
};

Action::Action(QObject* parent)
        : QObject(parent)
        , d(new Private)
{
    d->core = qobject_cast<PowerDevil::Core*>(parent);
}

Action::~Action()
{
    delete d;
}

void Action::registerIdleTimeout(int msec)
{
    d->registeredIdleTimeouts.append(msec);
    d->core->registerActionTimeout(this, msec);
}

bool Action::unloadAction()
{
    // Remove all registered idle timeouts, if any
    d->core->unregisterActionTimeouts(this);
    d->registeredIdleTimeouts.clear();

    // Ok, let's see if the action has to do something for being unloaded
    return onUnloadAction();
}

bool Action::onUnloadAction()
{
    // Usually nothing has to be done, so let's just happily return true
    return true;
}

bool Action::isSupported()
{
    return true;
}

BackendInterface* Action::backend()
{
    return d->core->backend();
}

Core* Action::core()
{
    return d->core;
}

void Action::trigger(const QVariantMap& args)
{
    if (args.contains("Explicit") && args["Explicit"].toBool()) {
        // The action was explicitly triggered by the user, hence any policy check is bypassed.
        triggerImpl(args);
    } else {
        // The action was taken automatically: let's check if we have the rights to do that
        PolicyAgent::RequiredPolicies unsatisfiablePolicies = PolicyAgent::instance()->requirePolicyCheck(d->requiredPolicies);
        if (unsatisfiablePolicies == PolicyAgent::None) {
            // Ok, let's trigger the action
            triggerImpl(args);
        } else {
            // TODO: Notify somehow?
            kWarning() << "Unsatisfied policies, the action has been aborted";
        }
    }
}

void Action::setRequiredPolicies(PolicyAgent::RequiredPolicies requiredPolicies)
{
    d->requiredPolicies = requiredPolicies;
}

}

#include "powerdevilaction.moc"
