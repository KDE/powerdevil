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

#include <KConfigGroup>
#include <KDebug>
#include <KGlobal>
#include <KServiceTypeTrader>
#include <KPluginInfo>

// Bundled actions:
#include "actions/bundled/suspendsession.h"
#include "actions/bundled/disabledesktopeffects.h"
#include "actions/bundled/brightnesscontrol.h"
#include "actions/bundled/dimdisplay.h"
#include "actions/bundled/runscript.h"
#include "actions/bundled/handlebuttonevents.h"

namespace PowerDevil
{

class ActionPoolHelper
{
public:
    ActionPoolHelper() : q(0) {}
    ~ActionPoolHelper() {
        delete q;
    }
    ActionPool *q;
};

K_GLOBAL_STATIC(ActionPoolHelper, s_globalActionPool)

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

}

Action* ActionPool::loadAction(const QString& actionId, const KConfigGroup& group, PowerDevil::Core *parent)
{
    // If it's cached, easy game.
    if (m_cachedPool.contains(actionId)) {
        Action *retaction = m_cachedPool[actionId];
        if (group.isValid()) {
            retaction->loadAction(group);
        }
        m_activeActions.append(actionId);
        return retaction;
    }

    // Is it one of the bundled actions?
    Action *retaction = 0;

    if (actionId == "SuspendSession") {
        retaction = new BundledActions::SuspendSession(parent);
    } else if (actionId == "DisableDesktopEffects") {
        retaction = new BundledActions::DisableDesktopEffects(parent);
    } else if (actionId == "BrightnessControl") {
        retaction = new BundledActions::BrightnessControl(parent);
    } else if (actionId == "DimDisplay") {
        retaction = new BundledActions::DimDisplay(parent);
    } else if (actionId == "RunScript") {
        retaction = new BundledActions::RunScript(parent);
    } else if (actionId == "ForceInhibition") {
        
    } else if (actionId == "HandleButtonEvents") {
        retaction = new BundledActions::HandleButtonEvents(parent);
    }

    if (retaction) {
        if (group.isValid()) {
            retaction->loadAction(group);
        }
        // Cache
        m_cachedPool.insert(actionId, retaction);
        m_activeActions.append(actionId);
        // Go
        return retaction;
    }

    // Otherwise, ask KService for the action itself
    KService::List offers = KServiceTypeTrader::self()->query("PowerDevil/Action");
    foreach (KService::Ptr offer, offers) {
        if (offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString() != actionId) {
            continue;
        }

        kDebug() << "Got a valid offer for " << actionId;
        //try to load the specified library
        retaction = offer->createInstance< PowerDevil::Action >(parent);

        if (!retaction) {
            // Troubles...
            kWarning() << "failed to load" << offer->desktopEntryName();
            break;
        }

        // All fine, let's break the cycle (cit.)
        break;
    }

    if (retaction) {
        if (group.isValid()) {
            retaction->loadAction(group);
        }
        // Cache
        m_cachedPool.insert(actionId, retaction);
        m_activeActions.append(actionId);
        // Go
        return retaction;
    }

    // Hmm... troubles in configuration. Np, let's just return 0 and let the core handle this
    return 0;
}

void ActionPool::unloadAllActiveActions()
{
    foreach (const QString &action, m_activeActions) {
        m_cachedPool[action]->onProfileUnload();
        m_cachedPool[action]->unloadAction();
    }
    m_activeActions.clear();
}

}
