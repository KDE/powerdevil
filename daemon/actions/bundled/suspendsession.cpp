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

#include <QDebug>

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
      m_dbusWatcher(0)
{
    // DBus
    new SuspendSessionAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::InterruptSession);

    connect(backend(), SIGNAL(resumeFromSuspend()),
            this, SLOT(onResumeFromSuspend()));
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
    qCDebug(POWERDEVIL) << "Triggered with " << args["Type"].toString() << args["SkipLocking"].toBool();

    // Switch for screen lock
    QVariantMap recallArgs;
    switch ((Mode) (args["Type"].toUInt())) {
        case ToRamMode:
        case ToDiskMode:
        case SuspendHybridMode:
            // Do we want to lock the screen?
            if (PowerDevilSettings::configLockScreen() && !args["SkipLocking"].toBool()) {
                // Yeah, we do.
                m_savedArgs = args;
                recallArgs["Type"] = (uint)LockScreenMode;
                triggerImpl(recallArgs);
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
    m_dbusWatcher = new QDBusPendingCallWatcher(reply, this);
    connect(m_dbusWatcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(lockCompleted()));
}

void SuspendSession::lockCompleted()
{
    QVariantMap args = m_savedArgs;

    m_dbusWatcher->deleteLater();
    m_dbusWatcher = 0;
    m_savedArgs.clear();

    args["SkipLocking"] = true;
    triggerImpl(args);
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

void SuspendSession::onResumeFromSuspend()
{
    // Notify the screensaver
    OrgFreedesktopScreenSaverInterface iface("org.freedesktop.ScreenSaver",
                                             "/ScreenSaver",
                                             QDBusConnection::sessionBus());
    iface.SimulateUserActivity();
    PowerDevil::PolicyAgent::instance()->setupSystemdInhibition();

    Q_EMIT resumingFromSuspend();
}

}
}

#include "suspendsession.moc"
