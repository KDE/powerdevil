/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilactionpool.h"

#include "powerdevil_debug.h"
#include "powerdevilaction.h"
#include "powerdevilcore.h"

#include <config-powerdevil.h>

#include <KConfigGroup>

#include <KPluginFactory>
#include <QDBusConnection>
#include <QDebug>

namespace PowerDevil
{
class ActionPoolHelper
{
public:
    ActionPoolHelper()
        : q(nullptr)
    {
    }
    ~ActionPoolHelper()
    {
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
    QHash<QString, Action *>::iterator i = m_actionPool.begin();
    while (i != m_actionPool.end()) {
        // Delete the associated action and erase
        i.value()->deleteLater();
        i = m_actionPool.erase(i);
    }
}

void ActionPool::init(PowerDevil::Core *parent)
{
    const QVector<KPluginMetaData> offers = KPluginMetaData::findPlugins(QStringLiteral("powerdevil/action"));
    for (const KPluginMetaData &data : offers) {
        if (auto plugin = KPluginFactory::instantiatePlugin<PowerDevil::Action>(data, parent).plugin) {
            m_actionPool.insert(data.value(QStringLiteral("X-KDE-PowerDevil-Action-ID")), plugin);
        }
    }

    // Verify support
    QHash<QString, Action *>::iterator i = m_actionPool.begin();
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
    for (const KPluginMetaData &offer : offers) {
        QString actionId = offer.value(QStringLiteral("X-KDE-PowerDevil-Action-ID"));
        if (offer.value(QStringLiteral("X-KDE-PowerDevil-Action-RegistersDBusInterface"), false) && m_actionPool.contains(actionId)) {
            QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/Solid/PowerManagement/Actions/") + actionId, m_actionPool[actionId]);
        }
    }
}

Action *ActionPool::loadAction(const QString &actionId, const KConfigGroup &group, PowerDevil::Core *parent)
{
    Q_UNUSED(parent);
    // Let's retrieve the action
    if (m_actionPool.contains(actionId)) {
        Action *retaction = m_actionPool[actionId];

        if (m_activeActions.contains(actionId)) {
            // We are reloading the action: let's unload it first then.
            retaction->onProfileUnload();
            retaction->unloadAction();
            m_activeActions.removeOne(actionId);
        }
        retaction->loadAction(group);
        m_activeActions.append(actionId);

        return retaction;
    } else {
        // Hmm... troubles in configuration. Np, let's just return 0 and let the core handle this
        return nullptr;
    }
}

Action *ActionPool::action(const QString &actionId) const
{
    return m_actionPool.value(actionId, nullptr);
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
