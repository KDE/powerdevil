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


#include "disabledesktopeffects.h"
#include <QtDBus/QDBusInterface>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QDBusPendingReply>

namespace PowerDevil {
namespace BundledActions {

DisableDesktopEffects::DisableDesktopEffects(QObject* parent)
    : Action(parent)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);
}

DisableDesktopEffects::~DisableDesktopEffects()
{

}

void DisableDesktopEffects::onProfileUnload()
{
    if (!m_hasIdleTime) {
        QVariantMap args;
        args["Enable"] = QVariant::fromValue(true);
        trigger(args);
    }
}

void DisableDesktopEffects::onWakeupFromIdle()
{
    QVariantMap args;
    args["Enable"] = QVariant::fromValue(true);
    trigger(args);
}

void DisableDesktopEffects::onIdleTimeout(int msec)
{
    Q_UNUSED(msec)    

    QVariantMap args;
    args["Enable"] = QVariant::fromValue(false);
    trigger(args);
}

void DisableDesktopEffects::onProfileLoad()
{
    if (!m_hasIdleTime) {
        QVariantMap args;
        args["Enable"] = QVariant::fromValue(false);
        trigger(args);
    }
}

void DisableDesktopEffects::triggerImpl(const QVariantMap& args)
{
    bool enabled = args["Enable"].toBool();
    QDBusInterface kwiniface("org.kde.kwin", "/KWin", "org.kde.KWin", QDBusConnection::sessionBus());

    QDBusPendingReply<bool> state = kwiniface.call("compositingActive");
    state.waitForFinished();

    if (state.value() != enabled) {
        kwiniface.asyncCall("toggleCompositing");
    } else {
    }
}

bool DisableDesktopEffects::loadAction(const KConfigGroup& config)
{
    if (config.readEntry<bool>("onIdle", false) && config.hasKey("idleTime")) {
        m_hasIdleTime = true;
        registerIdleTimeout(config.readEntry<int>("idleTime", 10000000));
    } else {
        m_hasIdleTime = false;
    }

    return true;
}

}
}

#include "disabledesktopeffects.moc"
