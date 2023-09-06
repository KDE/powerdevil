/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "suspendsession.h"

#include <powerdevil_debug.h>
#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>

#include "suspendsessionadaptor.h"

#include <kwinkscreenhelpereffect.h>

#include <KConfigGroup>
#include <KIdleTime>
#include <KJob>
#include <KLocalizedString>
#include <KPluginFactory>

#include <kworkspace.h>

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

void SuspendSession::onProfileUnload()
{
    // Nothing to do
}

void SuspendSession::onWakeupFromIdle()
{
    m_fadeEffect->stop();
}

void SuspendSession::onIdleTimeout(std::chrono::milliseconds timeout)
{
    QVariantMap args{{QStringLiteral("Type"), m_autoType}};

    // we fade the screen to black 5 seconds prior to suspending to alert the user
    if (timeout == m_idleTime - 5s) {
        args.insert(QStringLiteral("GraceFade"), true);
    } else {
        args.insert(QStringLiteral("SkipFade"), true);
    }

    trigger(args);
}

void SuspendSession::onProfileLoad(const QString & /*previousProfile*/, const QString & /*newProfile*/)
{
    // Nothing to do
}

void SuspendSession::triggerImpl(const QVariantMap &args)
{
    qCDebug(POWERDEVIL) << "Suspend session triggered with" << args;

    const auto mode = static_cast<PowerDevil::PowerButtonAction>(args["Type"].toUInt());

    switch (mode) {
    case PowerDevil::PowerButtonAction::SuspendToRam:
    case PowerDevil::PowerButtonAction::SuspendToDisk:
    case PowerDevil::PowerButtonAction::SuspendHybrid:
        // don't suspend if shutting down
        if (KWorkSpace::isShuttingDown()) {
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
    case PowerDevil::PowerButtonAction::SuspendToRam:
        Q_EMIT aboutToSuspend();

        if (m_suspendThenHibernateEnabled) {
            core()->suspendController()->suspendThenHibernate();
        } else {
            core()->suspendController()->suspend();
        }
        break;
    case PowerDevil::PowerButtonAction::SuspendToDisk:
        Q_EMIT aboutToSuspend();
        core()->suspendController()->hibernate();
        break;
    case PowerDevil::PowerButtonAction::SuspendHybrid:
        Q_EMIT aboutToSuspend();
        core()->suspendController()->hybridSuspend();
        break;
    case PowerDevil::PowerButtonAction::Shutdown:
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeHalt);
        break;
    case PowerDevil::PowerButtonAction::PromptLogoutDialog:
        KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmYes);
        break;
    case PowerDevil::PowerButtonAction::LockScreen: {
        // TODO should probably go through the backend (logind perhaps) eventually
        const QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver", "Lock");
        QDBusConnection::sessionBus().asyncCall(msg);
        break;
    }
    default:
        break;
    }
}

bool SuspendSession::loadAction(const KConfigGroup &config)
{
    if (config.isValid()) {
        if (config.hasKey("idleTime") && config.hasKey("suspendType")) {
            // Add the idle timeout
            m_idleTime = std::chrono::milliseconds(config.readEntry<int>("idleTime", 0));
            if (m_idleTime != 0ms) {
                registerIdleTimeout(m_idleTime - 5s);
                registerIdleTimeout(m_idleTime);
            }
            m_autoType = config.readEntry<uint>("suspendType", 0);
        }
        if (config.hasKey("suspendThenHibernate")) {
            m_suspendThenHibernateEnabled = config.readEntry<bool>("suspendThenHibernate", false);
        }
    }
    return true;
}

void SuspendSession::suspendHybrid()
{
    triggerSuspendSession(PowerDevil::PowerButtonAction::SuspendHybrid);
}

void SuspendSession::suspendToDisk()
{
    triggerSuspendSession(PowerDevil::PowerButtonAction::SuspendToDisk);
}

void SuspendSession::suspendToRam()
{
    triggerSuspendSession(PowerDevil::PowerButtonAction::SuspendToRam);
}

void SuspendSession::triggerSuspendSession(PowerDevil::PowerButtonAction action)
{
    trigger({
        {QStringLiteral("Type"), qToUnderlying(action)},
        {QStringLiteral("Explicit"), true},
    });
}

}
#include "suspendsession.moc"

#include "moc_suspendsession.cpp"
