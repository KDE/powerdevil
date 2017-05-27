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
#include "abstractdpmshelper.h"
#include "xcbdpmshelper.h"
#include "waylanddpmshelper.h"

#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>
#include <powerdevil_debug.h>

#include <QGuiApplication>
#include <QX11Info>
#include <QDebug>

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_FACTORY(PowerDevilDPMSActionFactory, registerPlugin<PowerDevilDPMSAction>(); )

PowerDevilDPMSAction::PowerDevilDPMSAction(QObject* parent, const QVariantList &args)
    : Action(parent)
    , m_helper()
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    if (QX11Info::isPlatformX11()) {
        m_helper.reset(new XcbDpmsHelper);
    } else if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive)) {
        m_helper.reset(new WaylandDpmsHelper);
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

PowerDevilDPMSAction::~PowerDevilDPMSAction() = default;

bool PowerDevilDPMSAction::isSupported()
{
    return !m_helper.isNull() && m_helper->isSupported();
}

void PowerDevilDPMSAction::onProfileUnload()
{
    if (m_helper.isNull()) {
        return;
    }
    m_helper->profileUnloaded();
}

void PowerDevilDPMSAction::onWakeupFromIdle()
{
    if (!m_helper.isNull()) {
        m_helper->stopFade();
    }
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
        if (!m_helper.isNull()) {
            m_helper->startFade();
        }
    } else if (msec == m_idleTime * 1000) {
        const int brightness = backend()->brightness(PowerDevil::BackendInterface::Keyboard);
        if (brightness > 0) {
            m_oldKeyboardBrightness = brightness;
            setKeyboardBrightnessHelper(0);
        }
        if (!m_helper.isNull()) {
            m_helper->dpmsTimeout();
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
    if (m_helper.isNull()) {
        return;
    }
    m_helper->profileLoaded(m_idleTime);
}

void PowerDevilDPMSAction::triggerImpl(const QVariantMap& args)
{
    QString KEYBOARD_BRIGHTNESS = QStringLiteral("KeyboardBrightness");
    if (args.contains(KEYBOARD_BRIGHTNESS)) {
        core()->setBrightness(args.value(KEYBOARD_BRIGHTNESS).toInt(), PowerDevil::BackendInterface::Keyboard);
        return;
    }

    if (m_helper.isNull()) {
        return;
    }
    m_helper->trigger(args.value(QStringLiteral("Type")).toString());
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
        if (!m_helper.isNull()) {
            m_helper->inhibited();
        }
    } else {
        // Inhibition removed: let's start again
        onProfileLoad();
        qCDebug(POWERDEVIL) << "Restoring DPMS features after inhibition release";
    }
}

#include "powerdevildpmsaction.moc"
