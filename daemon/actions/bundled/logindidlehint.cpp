/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "logindidlehint.h"
#include "logindidlehintclient.h"

#include <PowerDevilGlobalSettings.h>
#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilcore.h>

#include <KPluginFactory>

#include <QDBusConnection>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::LogindIdleHint, "powerdevillogindidlehintaction.json")

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace PowerDevil::BundledActions
{
LogindIdleHint::LogindIdleHint(QObject *parent)
    : Action(parent)
    , m_client(new LogindIdleHintClient(QDBusConnection::systemBus(), u"org.freedesktop.login1"_s, this))
{
}

bool LogindIdleHint::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)
    // A new profile starts from activity: the wake-up of the previous one does not reach this action.
    m_client->setIdle(false);

    const std::chrono::seconds timeout(core()->globalSettings()->sessionIdleTimeoutSec());
    if (timeout <= 0s) {
        return false;
    }

    qCDebug(POWERDEVIL) << "LogindIdleHint: the session is idle after" << timeout;
    registerIdleTimeout(timeout);
    return true;
}

void LogindIdleHint::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_UNUSED(timeout)
    m_client->setIdle(true);
}

void LogindIdleHint::onWakeupFromIdle()
{
    m_client->setIdle(false);
}

void LogindIdleHint::onProfileUnload()
{
    m_client->setIdle(false);
}
}

#include "logindidlehint.moc"

#include "moc_logindidlehint.cpp"
