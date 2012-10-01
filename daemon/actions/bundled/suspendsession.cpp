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

#include <KConfigGroup>
#include <KLocalizedString>
#include <KJob>
#include <KDebug>

#include <kworkspace/kworkspace.h>

#include "screensaver_interface.h"
#include <PowerDevilSettings.h>

namespace PowerDevil
{
namespace BundledActions
{

SuspendSession::SuspendSession(QObject* parent)
    : Action(parent)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::InterruptSession);
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
    kDebug() << "Triggered with " << args["Type"].toString();

    // Switch for screen lock
    QVariantMap recallArgs;
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
        case ToDiskMode:
        case SuspendHybridMode:
            // Do we want to lock the screen?
            if (PowerDevilSettings::configLockScreen()) {
                // Yeah, we do.
                recallArgs["Type"] = (uint)LockScreenMode;
                triggerImpl(recallArgs);
            }
            break;
        default:
            break;
    }

    // Switch for real action
    KJob *suspendJob = 0;
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::ToRam);
            break;
        case ToDiskMode:
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::ToDisk);
            break;
        case SuspendHybridMode:
            suspendJob = backend()->suspend(PowerDevil::BackendInterface::HybridSuspend);
            break;
        case ShutdownMode:
            KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmNo, KWorkSpace::ShutdownTypeHalt);
            break;
        case LogoutDialogMode:
            KWorkSpace::requestShutDown(KWorkSpace::ShutdownConfirmYes);
            break;
        case LockScreenMode:
            core()->emitNotification("lockscreen", i18n("Locking Screen"), i18n("The screen is being locked"));
            lockScreenAndWait();
            break;
        default:
            break;
    }

    if (suspendJob) {
        suspendJob->start();
    }
}

void SuspendSession::lockScreenAndWait()
{
    OrgFreedesktopScreenSaverInterface iface("org.freedesktop.ScreenSaver",
                                             "/ScreenSaver",
                                             QDBusConnection::sessionBus());
    QDBusPendingReply< void > reply = iface.Lock();
    reply.waitForFinished();
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

}
}

#include "suspendsession.moc"
