/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "suspendsession.h"

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>
#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>

#include "suspendsessionadaptor.h"

#include <kwinkscreenhelpereffect.h>

#include <KIdleTime>
#include <KJob>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::SuspendSession, "powerdevilsuspendsessionaction.json")

using namespace std::chrono_literals;

namespace PowerDevil::BundledActions
{
SuspendSession::SuspendSession(QObject *parent)
    : Action(parent)
    , m_fadeEffect(new PowerDevil::KWinKScreenHelperEffect())
{
    // DBus
    new SuspendSessionAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::InterruptSession);

    connect(core()->suspendController(), &SuspendController::resumeFromSuspend, this, [this]() {
        KIdleTime::instance()->simulateUserActivity();

        PowerDevil::PolicyAgent::instance()->setupSystemdInhibition();

        m_fadeEffect->stop();

        Q_EMIT resumingFromSuspend();
    });

    connect(m_fadeEffect.data(), &PowerDevil::KWinKScreenHelperEffect::fadedOut, this, [this]() {
        if (!m_savedArgs.isEmpty()) {
            QVariantMap args = m_savedArgs;
            args["SkipFade"] = true;
            triggerImpl(args);
        }
    });
}

SuspendSession::~SuspendSession() = default;

void SuspendSession::onWakeupFromIdle()
{
    m_fadeEffect->stop();
}

void SuspendSession::onIdleTimeout(std::chrono::milliseconds timeout)
{
    QVariantMap args{{QStringLiteral("Type"), qToUnderlying(m_autoSuspendAction)}};

    // we fade the screen to black 5 seconds prior to suspending to alert the user
    if (timeout == m_idleTime - 5s) {
        args.insert(QStringLiteral("GraceFade"), true);
    } else {
        args.insert(QStringLiteral("SkipFade"), true);
    }

    trigger(args);
}

void SuspendSession::triggerImpl(const QVariantMap &args)
{
    qCDebug(POWERDEVIL) << "Suspend session triggered with" << args;

    const auto mode = static_cast<PowerDevil::PowerButtonAction>(args["Type"].toUInt());

    switch (mode) {
    case PowerDevil::PowerButtonAction::SuspendToRam:
    case PowerDevil::PowerButtonAction::SuspendToDisk:
        // don't suspend if shutting down
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.Shutdown"))) {
            qCDebug(POWERDEVIL) << "Not suspending because a shutdown is in progress";
            return;
        }

        if (!args["SkipFade"].toBool()) {
            m_savedArgs = args;
            m_fadeEffect->start();
            return;
        }
    default:
        /* no suspend action => no fade effect */;
    }

    if (args["GraceFade"].toBool()) {
        return;
    }

    // Switch for real action
    switch (static_cast<PowerDevil::PowerButtonAction>(args["Type"].toUInt())) {
    case PowerDevil::PowerButtonAction::SuspendToRam: {
        Q_EMIT aboutToSuspend();
        auto sleepMode = args.contains("SleepMode") ? static_cast<PowerDevil::SleepMode>(args["SleepMode"].toUInt()) : m_sleepMode;

        if (sleepMode == PowerDevil::SleepMode::SuspendThenHibernate) {
            core()->suspendController()->suspendThenHibernate();
        } else if (sleepMode == PowerDevil::SleepMode::HybridSuspend) {
            core()->suspendController()->hybridSuspend();
        } else {
            core()->suspendController()->suspend();
        }
        break;
    }
    case PowerDevil::PowerButtonAction::SuspendToDisk: {
        Q_EMIT aboutToSuspend();
        core()->suspendController()->hibernate();
        break;
    }
    case PowerDevil::PowerButtonAction::Shutdown: {
        SessionManagement sessionManager;
        sessionManager.requestShutdown(SessionManagement::ConfirmationMode::Skip);
        break;
    }
    case PowerDevil::PowerButtonAction::PromptLogoutDialog: {
        SessionManagement sessionManager;
        sessionManager.requestShutdown(SessionManagement::ConfirmationMode::ForcePrompt);
        break;
    }
    case PowerDevil::PowerButtonAction::LockScreen: {
        SessionManagement sessionManager;
        sessionManager.lock();
        break;
    }
    default:
        break;
    }
}

bool SuspendSession::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    m_sleepMode = static_cast<PowerDevil::SleepMode>(profileSettings.sleepMode());

    if (profileSettings.autoSuspendAction() == qToUnderlying(PowerDevil::PowerButtonAction::NoAction)) {
        return false;
    }

    // Add the idle timeout
    m_idleTime = std::chrono::seconds(profileSettings.autoSuspendIdleTimeoutSec());
    if (m_idleTime != 0s) {
        registerIdleTimeout(m_idleTime - 5s);
        registerIdleTimeout(m_idleTime);
    }
    m_autoSuspendAction = static_cast<PowerDevil::PowerButtonAction>(profileSettings.autoSuspendAction());

    return true;
}

void SuspendSession::suspendToDisk()
{
    triggerSuspendSession(PowerDevil::PowerButtonAction::SuspendToDisk);
}

void SuspendSession::suspendToRam()
{
    triggerImpl({
        {QStringLiteral("Type"), qToUnderlying(PowerDevil::PowerButtonAction::SuspendToRam)},
        {QStringLiteral("SleepMode"), qToUnderlying(PowerDevil::SleepMode::SuspendToRam)},
    });
}

void SuspendSession::suspendHybrid()
{
    triggerImpl({
        {QStringLiteral("Type"), qToUnderlying(PowerDevil::PowerButtonAction::SuspendToRam)},
        {QStringLiteral("SleepMode"), qToUnderlying(PowerDevil::SleepMode::HybridSuspend)},
    });
}

void SuspendSession::triggerSuspendSession(PowerDevil::PowerButtonAction action)
{
    triggerImpl({
        {QStringLiteral("Type"), qToUnderlying(action)},
    });
}
}
#include "suspendsession.moc"

#include "moc_suspendsession.cpp"
