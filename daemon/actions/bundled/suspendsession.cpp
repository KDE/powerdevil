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

#include "suspendsession.h"

#include "powerdevilbackendinterface.h"
#include "powerdevilcore.h"
#include <powerdevil_debug.h>

#include "suspendsessionadaptor.h"

#include <kwinkscreenhelpereffect.h>

#include <KConfigGroup>
#include <KIdleTime>
#include <KJob>
#include <KLocalizedString>
#include <KPluginFactory>

#include <kworkspace.h>

#include <PowerDevilSettings.h>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::SuspendSession, "powerdevilsuspendsessionaction.json")

namespace PowerDevil::BundledActions
{
SuspendSession::SuspendSession(QObject *parent)
    : Action(parent)
    , m_fadeEffect(new PowerDevil::KWinKScreenHelperEffect())
{
    // DBus
    new SuspendSessionAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::InterruptSession);

    connect(backend(), &PowerDevil::BackendInterface::resumeFromSuspend, this, [this]() {
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

void SuspendSession::onIdleTimeout(int msec)
{
    QVariantMap args{
        {QStringLiteral("Type"), m_autoType}
    };

    // we fade the screen to black 5 seconds prior to suspending to alert the user
    if (msec == m_idleTime - 5000) {
        args.insert(QStringLiteral("GraceFade"), true);
    } else {
        args.insert(QStringLiteral("SkipFade"), true);
    }

    trigger(args);
}

void SuspendSession::onProfileLoad(const QString &/*previousProfile*/, const QString &/*newProfile*/)
{
    // Nothing to do
}

void SuspendSession::triggerImpl(const QVariantMap &args)
{
    qCDebug(POWERDEVIL) << "Suspend session triggered with" << args;

    const auto mode = static_cast<Mode>(args["Type"].toUInt());

    if (mode == ToRamMode || mode == ToDiskMode || mode == SuspendHybridMode) {
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
    }

    if (args["GraceFade"].toBool()) {
        return;
    }

    // Switch for real action
    KJob *suspendJob = nullptr;
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
            Q_EMIT aboutToSuspend();
            suspendJob = backend()->suspend(m_suspendThenHibernateEnabled
                ? PowerDevil::BackendInterface::SuspendThenHibernate : PowerDevil::BackendInterface::ToRam);
            break;
        case ToDiskMode:
            Q_EMIT aboutToSuspend();
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::ToDisk);
            break;
        case SuspendHybridMode:
            Q_EMIT aboutToSuspend();
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::HybridSuspend);
            break;
        case ShutdownMode:
            KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeHalt);
            break;
        case LogoutDialogMode:
            KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmYes);
            break;
        case LockScreenMode: {
            // TODO should probably go through the backend (logind perhaps) eventually
            QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver",
                                                                                   "/ScreenSaver",
                                                                                   "org.freedesktop.ScreenSaver",
                                                                                   "Lock"));
            break;
        }
        default:
            break;
    }

    if (suspendJob) {
        // TODO connect(suspendJob, &KJob::error ??, this, [this]() { m_fadeEffect->stop(); });
        suspendJob->start();
    }
}

bool SuspendSession::loadAction(const KConfigGroup& config)
{
    if (config.isValid()) {
        if (config.hasKey("idleTime") && config.hasKey("suspendType")) {
            // Add the idle timeout
            m_idleTime = config.readEntry<int>("idleTime", 0);
            if (m_idleTime) {
                registerIdleTimeout(m_idleTime - 5000);
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
    triggerSuspendSession(SuspendHybridMode);
}

void SuspendSession::suspendToDisk()
{
    triggerSuspendSession(ToDiskMode);
}

void SuspendSession::suspendToRam()
{
    triggerSuspendSession(ToRamMode);
}

void SuspendSession::triggerSuspendSession(uint action)
{
    trigger({
        {QStringLiteral("Type"), action},
        {QStringLiteral("Explicit"), true},
    });
}

}
#include "suspendsession.moc"
