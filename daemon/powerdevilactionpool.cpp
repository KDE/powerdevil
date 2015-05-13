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

#include <KConfigGroup>
#include <KServiceTypeTrader>
#include <KPluginInfo>

#include <QDBusConnection>
#include <QDebug>

// Bundled actions:
#include "actions/bundled/suspendsession.h"
#include "actions/bundled/brightnesscontrol.h"
#include "actions/bundled/keyboardbrightnesscontrol.h"
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
    // Load all the actions
    const KService::List offers = KServiceTypeTrader::self()->query("PowerDevil/Action",
                                                                    "[X-KDE-PowerDevil-Action-IsBundled] == FALSE");
    foreach (const KService::Ptr &offer, offers) {
        const QString actionId = offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString();

        qCDebug(POWERDEVIL) << "Got a valid offer for " << actionId;
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
    m_actionPool.insert("SuspendSession", new BundledActions::SuspendSession(parent));
    m_actionPool.insert("BrightnessControl", new BundledActions::BrightnessControl(parent));
    m_actionPool.insert("KeyboardBrightnessControl", new BundledActions::KeyboardBrightnessControl(parent));
    m_actionPool.insert("DimDisplay", new BundledActions::DimDisplay(parent));
    m_actionPool.insert("RunScript", new BundledActions::RunScript(parent));
    m_actionPool.insert("HandleButtonEvents", new BundledActions::HandleButtonEvents(parent));

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
        const KService::List offers = KServiceTypeTrader::self()->query("PowerDevil/Action",
                                                                        "[X-KDE-PowerDevil-Action-RegistersDBusInterface] == TRUE");
        foreach (const KService::Ptr &offer, offers) {
            QString actionId = offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString();

            if (m_actionPool.contains(actionId)) {
                QDBusConnection::sessionBus().registerObject("/org/kde/Solid/PowerManagement/Actions/" + actionId, m_actionPool[actionId]);
            }
        }
    }
}

Action* ActionPool::loadAction(const QString& actionId, const KConfigGroup& group, PowerDevil::Core *parent)
{
    Q_UNUSED(parent);
    // Let's retrieve the action
    if (m_actionPool.contains(actionId)) {
        Action *retaction = m_actionPool[actionId];

        if (group.isValid()) {

            if (m_activeActions.contains(actionId)) {
                // We are reloading the action: let's unload it first then.
                retaction->onProfileUnload();
                retaction->unloadAction();
                m_activeActions.removeOne(actionId);
            }

            retaction->loadAction(group);
            m_activeActions.append(actionId);
        }

        return retaction;
    } else {
        // Hmm... troubles in configuration. Np, let's just return 0 and let the core handle this
        return 0;
    }
}

void ActionPool::unloadAllActiveActions()
{
    foreach (const QString &action, m_activeActions) {
        m_actionPool[action]->onProfileUnload();
        m_actionPool[action]->unloadAction();
    }
    m_activeActions.clear();
}

}
