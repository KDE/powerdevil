/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sessionidlehint.h"

#include <PowerDevilGlobalSettings.h>
#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilcore.h>
#include <powerdevilpolicyagent.h>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::SessionIdleHint, "powerdevilsessionidlehintaction.json")

using namespace std::chrono_literals;

namespace PowerDevil::BundledActions
{
SessionIdleHint::SessionIdleHint(QObject *parent)
    : Action(parent)
{
}

bool SessionIdleHint::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)
    // A new profile starts from activity: the wake-up of the previous one does not reach this action.
    PolicyAgent::instance()->setSessionIdleHint(false);

    const std::chrono::seconds timeout(core()->globalSettings()->sessionIdleTimeoutSec());
    if (timeout <= 0s) {
        return false;
    }

    qCDebug(POWERDEVIL) << "SessionIdleHint: the session is idle after" << timeout;
    registerIdleTimeout(timeout);
    return true;
}

void SessionIdleHint::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_UNUSED(timeout)
    PolicyAgent::instance()->setSessionIdleHint(true);
}

void SessionIdleHint::onWakeupFromIdle()
{
    PolicyAgent::instance()->setSessionIdleHint(false);
}

void SessionIdleHint::onProfileUnload()
{
    PolicyAgent::instance()->setSessionIdleHint(false);
}
}

#include "sessionidlehint.moc"

#include "moc_sessionidlehint.cpp"
