/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilaction.h"

#include "powerdevil_debug.h"
#include "powerdevilcore.h"

#include <QDebug>

namespace PowerDevil
{
class Action::Private
{
public:
    Private()
    {
    }
    ~Private()
    {
    }

    PowerDevil::Core *core;

    QVector<std::chrono::milliseconds> registeredIdleTimeouts;
    PowerDevil::PolicyAgent::RequiredPolicies requiredPolicies;
};

Action::Action(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->core = qobject_cast<PowerDevil::Core *>(parent);
}

Action::~Action()
{
    delete d;
}

void Action::registerIdleTimeout(std::chrono::milliseconds timeout)
{
    d->registeredIdleTimeouts.append(timeout);
    d->core->registerActionTimeout(this, timeout);
}

void Action::unloadAction()
{
    // Remove all registered idle timeouts, if any
    d->core->unregisterActionTimeouts(this);
    d->registeredIdleTimeouts.clear();
}

bool Action::isSupported()
{
    return true;
}

BackendInterface *Action::backend() const
{
    return d->core->backend();
}

Core *Action::core()
{
    return d->core;
}

void Action::trigger(const QVariantMap &args)
{
    if (args.contains(QStringLiteral("Explicit")) && args[QStringLiteral("Explicit")].toBool()) {
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
            qCWarning(POWERDEVIL) << "Unsatisfied policies, the action has been aborted";
        }
    }
}

void Action::setRequiredPolicies(PolicyAgent::RequiredPolicies requiredPolicies)
{
    d->requiredPolicies = requiredPolicies;
}

}

#include "moc_powerdevilaction.cpp"
