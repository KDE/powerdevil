/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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


#include "powerdevildpmsaction.h"

#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>
#include <powerdevil_debug.h>

#include <config-workspace.h>

#include <kwinkscreenhelpereffect.h>

#include <QX11Info>
#include <QDebug>

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>

#include <xcb/dpms.h>

K_PLUGIN_FACTORY(PowerDevilDPMSActionFactory, registerPlugin<PowerDevilDPMSAction>(); )

PowerDevilDPMSAction::PowerDevilDPMSAction(QObject* parent, const QVariantList &args)
    : Action(parent)
    , m_fadeEffect(new PowerDevil::KWinKScreenHelperEffect())
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    ScopedCPointer<xcb_dpms_capable_reply_t> capableReply(xcb_dpms_capable_reply(QX11Info::connection(),
        xcb_dpms_capable(QX11Info::connection()),
    nullptr));

    if (capableReply && capableReply->capable) {
        m_supported = true;
    }

    // Is the action being loaded outside the core?
    if (args.size() > 0) {
        if (args.first().toBool()) {
            qCDebug(POWERDEVIL) << "Action loaded from outside the core, skipping early init";
            return;
        }
    }

    // Pretend we're unloading profiles here, as if the action is not enabled, DPMS should be switched off.
    onProfileUnload();

    // Listen to the policy agent
    connect(PowerDevil::PolicyAgent::instance(), &PowerDevil::PolicyAgent::unavailablePoliciesChanged,
            this, &PowerDevilDPMSAction::onUnavailablePoliciesChanged);

    // inhibitions persist over kded module unload/load
    m_inhibitScreen = PowerDevil::PolicyAgent::instance()->unavailablePolicies() & PowerDevil::PolicyAgent::ChangeScreenSettings;
}

bool PowerDevilDPMSAction::isSupported()
{
    return m_supported;
}

void PowerDevilDPMSAction::onProfileUnload()
{
    using namespace PowerDevil;

    if (!(PolicyAgent::instance()->unavailablePolicies() & PolicyAgent::ChangeScreenSettings)) {
        xcb_dpms_disable(QX11Info::connection());
    } else {
        qCDebug(POWERDEVIL) << "Not performing DPMS action due to inhibition";
    }

    xcb_dpms_set_timeouts(QX11Info::connection(), 0, 0, 0);
}

void PowerDevilDPMSAction::onWakeupFromIdle()
{
    m_fadeEffect->stop();
    if (m_oldKeyboardBrightness > 0) {
        setKeyboardBrightnessHelper(m_oldKeyboardBrightness);
        m_oldKeyboardBrightness = 0;
    }
}

void PowerDevilDPMSAction::onIdleTimeout(int msec)
{
    // Do not inhibit anything even if idleTimeout reaches because we are inhibit
    if (m_inhibitScreen) {
        return;
    }

    if (msec == m_idleTime * 1000 - 5000) { // fade out screen
        m_fadeEffect->start();
    } else if (msec == m_idleTime * 1000) {
        const int brightness = backend()->brightness(PowerDevil::BackendInterface::Keyboard);
        if (brightness > 0) {
            m_oldKeyboardBrightness = brightness;
            setKeyboardBrightnessHelper(0);
        }
    }
}

void PowerDevilDPMSAction::setKeyboardBrightnessHelper(int brightness)
{
    trigger({
        {"KeyboardBrightness", QVariant::fromValue(brightness)}
    });
}

void PowerDevilDPMSAction::onProfileLoad()
{
    using namespace PowerDevil;

    if (!(PolicyAgent::instance()->unavailablePolicies() & PolicyAgent::ChangeScreenSettings)) {
        xcb_dpms_enable(QX11Info::connection());
    } else {
        qCDebug(POWERDEVIL) << "Not performing DPMS action due to inhibition";
        return;
    }

    // An unloaded action will have m_idleTime = 0:
    // DPMS enabled with zeroed timeouts is effectively disabled.
    // So onProfileLoad is always safe
    xcb_dpms_set_timeouts(QX11Info::connection(), m_idleTime, m_idleTime * 1.5, m_idleTime * 2);
}

void PowerDevilDPMSAction::triggerImpl(const QVariantMap& args)
{
    if (args.contains("KeyboardBrightness")) {
        backend()->setBrightness(args.value(QStringLiteral("KeyboardBrightness")).toInt(), PowerDevil::BackendInterface::Keyboard);
        return;
    }

    const QString type = args.value(QStringLiteral("Type")).toString();
    int level = 0;

    if (type == QLatin1String("TurnOff")) {
        level = XCB_DPMS_DPMS_MODE_OFF;
    } else if (type == QLatin1String("Standby")) {
        level = XCB_DPMS_DPMS_MODE_STANDBY;
    } else if (type == QLatin1String("Suspend")) {
        level = XCB_DPMS_DPMS_MODE_SUSPEND;
    } else {
        // this leaves DPMS enabled but if it's meant to be disabled
        // then the timeouts will be zero and so effectively disabled
        return;
    }

    ScopedCPointer<xcb_dpms_info_reply_t> infoReply(xcb_dpms_info_reply(QX11Info::connection(),
        xcb_dpms_info(QX11Info::connection()),
    nullptr));

    if (!infoReply) {
        qCWarning(POWERDEVIL) << "Failed to query DPMS state, cannot trigger" << level;
        return;
    }

    if (!infoReply->state) {
        xcb_dpms_enable(QX11Info::connection());
    }

    xcb_dpms_force_level(QX11Info::connection(), level);
}

bool PowerDevilDPMSAction::loadAction(const KConfigGroup& config)
{
    m_idleTime = config.readEntry<int>("idleTime", -1);
    if (m_idleTime > 0) {
        registerIdleTimeout(m_idleTime * 1000);
        registerIdleTimeout(m_idleTime * 1000 - 5000); // start screen fade a bit earlier to alert user
    }

    return true;
}

bool PowerDevilDPMSAction::onUnloadAction()
{
    m_idleTime = 0;
    return Action::onUnloadAction();
}

void PowerDevilDPMSAction::onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies)
{
    // only take action if screen inhibit changed
    PowerDevil::PolicyAgent::RequiredPolicies oldPolicy = m_inhibitScreen;
    m_inhibitScreen = policies & PowerDevil::PolicyAgent::ChangeScreenSettings;
    if (oldPolicy == m_inhibitScreen) {
        return;
    }

    if (m_inhibitScreen) {
        // Inhibition triggered: disable DPMS
        qCDebug(POWERDEVIL) << "Disabling DPMS due to inhibition";
        xcb_dpms_set_timeouts(QX11Info::connection(), 0, 0, 0);
        xcb_dpms_disable(QX11Info::connection()); // wakes the screen - do we want this?
    } else {
        // Inhibition removed: let's start again
        onProfileLoad();
        qCDebug(POWERDEVIL) << "Restoring DPMS features after inhibition release";
    }
}

#include "powerdevildpmsaction.moc"
