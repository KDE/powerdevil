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
#include <KLocalizedString>
#include <KJob>

#include <kworkspace.h>

#include "screensaver_interface.h"
#include <PowerDevilSettings.h>

namespace PowerDevil
{
namespace BundledActions
{

SuspendSession::SuspendSession(QObject* parent)
    : Action(parent),
      m_fadeEffect(new PowerDevil::KWinKScreenHelperEffect())
{
    // DBus
    new SuspendSessionAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::InterruptSession);

    connect(backend(), &PowerDevil::BackendInterface::resumeFromSuspend, this, [this]() {
        // Notify the screensaver
        OrgFreedesktopScreenSaverInterface iface("org.freedesktop.ScreenSaver", "/ScreenSaver", QDBusConnection::sessionBus());
        iface.SimulateUserActivity();
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

SuspendSession::~SuspendSession()
{

}

void SuspendSession::onProfileUnload()
{
    // Nothing to do
}

void SuspendSession::onWakeupFromIdle()
{
    // Nothing to do
}

void SuspendSession::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
    QVariantMap args;
    args.insert("Type", m_autoType);
    trigger(args);
}

void SuspendSession::onProfileLoad()
{
    // Nothing to do
}

void SuspendSession::triggerImpl(const QVariantMap& args)
{
    qCDebug(POWERDEVIL) << "Triggered with " << args["Type"].toString() << args["SkipFade"].toBool();

    // Switch for screen fading
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
        case ToDiskMode:
        case SuspendHybridMode:
            if (!args["SkipFade"].toBool()) {
                m_savedArgs = args;
                m_fadeEffect->start();
                return;
            }
            break;
        default:
            break;
    }

    // Switch for real action
    KJob *suspendJob = 0;
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
            Q_EMIT aboutToSuspend();
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::ToRam);
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
            OrgFreedesktopScreenSaverInterface iface("org.freedesktop.ScreenSaver", "/ScreenSaver", QDBusConnection::sessionBus());
            iface.Lock();
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
    if (config.isValid() && config.hasKey("idleTime") && config.hasKey("suspendType")) {
        // Add the idle timeout
        registerIdleTimeout(config.readEntry<int>("idleTime", 0));
        m_autoType = config.readEntry<uint>("suspendType", 0);
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
    QVariantMap args;
    args["Type"] = action;
    args["Explicit"] = true;
    trigger(args);
}

}
}

#include "suspendsession.moc"
