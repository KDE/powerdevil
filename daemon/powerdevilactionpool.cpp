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


#include "powerdevilactionpool.h"

#include "powerdevilaction.h"
#include "powerdevilcore.h"
#include "powerdevil_debug.h"

#include <config-powerdevil.h>

#include <KServiceTypeTrader>

#include <QDBusConnection>
#include <QDebug>

// Bundled actions:
#include "actions/bundled/suspendsession.h"
#include "actions/bundled/brightnesscontrol.h"
#include "actions/bundled/keyboardbrightnesscontrol.h"
#include "actions/bundled/dimdisplay.h"
#include "actions/bundled/runscript.h"
#include "actions/bundled/handlebuttonevents.h"
#include "actions/bundled/dpms.h"
#ifdef HAVE_WIRELESS_SUPPORT
#include "actions/bundled/wirelesspowersaving.h"
#endif
#include "actions/bundled/powerprofile.h"

#include <PowerDevilProfileSettings.h>

namespace PowerDevil
{

class ActionPoolHelper
{
public:
    ActionPoolHelper() : q(nullptr) {}
    ~ActionPoolHelper() {
        delete q;
    }
    ActionPool *q;
};

Q_GLOBAL_STATIC(ActionPoolHelper, s_globalActionPool)

ActionPool *ActionPool::instance()
{
    if (!s_globalActionPool->q) {
        new ActionPool;
    }

    return s_globalActionPool->q;
}

ActionPool::ActionPool()
{
    Q_ASSERT(!s_globalActionPool->q);
    s_globalActionPool->q = this;
}

ActionPool::~ActionPool()
{
    clearCache();
}

void ActionPool::clearCache()
{
    QHash< QString, Action* >::iterator i = m_actionPool.begin();
    while (i != m_actionPool.end()) {
        // Delete the associated action and erase
        i.value()->deleteLater();
        i = m_actionPool.erase(i);
    }
}

void ActionPool::init(PowerDevil::Core *parent)
{
    //TODO:
    // We removed all the non bundled actions, should we delete this code?
    // Load all the actions
    const KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("PowerDevil/Action"),
                                                              QStringLiteral("[X-KDE-PowerDevil-Action-IsBundled] == FALSE"));
    for (const KService::Ptr &offer : offers) {
        QString actionId = offer->property(QStringLiteral("X-KDE-PowerDevil-Action-ID"), QVariant::String).toString();

        qCDebug(POWERDEVIL) << "Got a valid offer for " << actionId;

        if (!offer->showOnCurrentPlatform()) {
            qCDebug(POWERDEVIL) << "Doesn't support the windowing system";
            continue;
        }

        //try to load the specified library
        PowerDevil::Action *retaction = offer->createInstance< PowerDevil::Action >(parent);

        if (!retaction) {
            // Troubles...
            qCWarning(POWERDEVIL) << "failed to load" << offer->desktopEntryName();
            continue;
        }

        // Is the action available and supported?
        if (!retaction->isSupported()) {
            // Skip that
            retaction->deleteLater();
            continue;
        }

        // Insert
        m_actionPool.insert(actionId, retaction);
    }

    // Load bundled actions now
    m_actionPool.insert(QStringLiteral("SuspendSession"), new BundledActions::SuspendSession(parent));
    m_actionPool.insert(QStringLiteral("BrightnessControl"), new BundledActions::BrightnessControl(parent));
    m_actionPool.insert(QStringLiteral("KeyboardBrightnessControl"), new BundledActions::KeyboardBrightnessControl(parent));
    m_actionPool.insert(QStringLiteral("DimDisplay"), new BundledActions::DimDisplay(parent));
    m_actionPool.insert(QStringLiteral("RunScript"), new BundledActions::RunScript(parent));
    m_actionPool.insert(QStringLiteral("HandleButtonEvents"), new BundledActions::HandleButtonEvents(parent));
    m_actionPool.insert(QStringLiteral("DPMSControl"), new BundledActions::DPMS(parent));
#ifdef HAVE_WIRELESS_SUPPORT
    m_actionPool.insert(QStringLiteral("WirelessPowerSaving"), new BundledActions::WirelessPowerSaving(parent));
#endif
    m_actionPool.insert(QStringLiteral("PowerProfile"), new BundledActions::PowerProfile(parent));

    // Verify support
    QHash<QString,Action*>::iterator i = m_actionPool.begin();
    while (i != m_actionPool.end()) {
        Action *action = i.value();
        if (!action->isSupported()) {
            i = m_actionPool.erase(i);
            action->deleteLater();
        } else {
            ++i;
        }
    }

    // Register DBus objects
    {
        const KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("PowerDevil/Action"),
                                                                QStringLiteral("[X-KDE-PowerDevil-Action-RegistersDBusInterface] == TRUE"));
        for (const KService::Ptr &offer : offers) {
            QString actionId = offer->property(QStringLiteral("X-KDE-PowerDevil-Action-ID"), QVariant::String).toString();

            if (m_actionPool.contains(actionId)) {
                QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/Solid/PowerManagement/Actions/") + actionId, m_actionPool[actionId]);
            }
        }
    }
}

void ActionPool::loadActionsForProfile(PowerDevilProfileSettings* settings, const QString& profileId, PowerDevil::Core* parent)
{
    for (const QString &actionName : m_actionPool.keys()) {
        Action *action = ActionPool::instance()->loadAction(actionName, settings, parent);
        if (action) {
            action->onProfileLoad();
            continue;
        }
        qCWarning(POWERDEVIL)
            << "The profile " << profileId <<  "tried to activate"
            << actionName << "a non-existent action. This is usually due to an installation problem,"
            << " a configuration problem, or because the action is not supported";
    }
}

Action* ActionPool::loadAction(const QString& actionId, PowerDevilProfileSettings *settings, PowerDevil::Core *parent)
{
    Q_UNUSED(parent);
    // Let's retrieve the action
    if (!m_actionPool.contains(actionId)) {
        return nullptr;
    }
    Action *retaction = m_actionPool[actionId];

    if (settings) {
        if (m_activeActions.contains(actionId)) {
            // We are reloading the action: let's unload it first then.
            retaction->onProfileUnload();
            retaction->unloadAction();
            m_activeActions.removeOne(actionId);
        }

        retaction->loadAction(settings);
        m_activeActions.append(actionId);
    }

    return retaction;
}

void ActionPool::unloadAllActiveActions()
{
    for (const QString &action : qAsConst(m_activeActions)) {
        m_actionPool[action]->onProfileUnload();
        m_actionPool[action]->unloadAction();
    }
    m_activeActions.clear();
}

}
