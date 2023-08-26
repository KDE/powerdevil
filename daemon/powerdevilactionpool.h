/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QStringList>

#include "powerdevilcore_export.h"

class KConfigGroup;
namespace PowerDevil
{
class Core;
class Action;

class POWERDEVILCORE_EXPORT ActionPool
{
public:
    static ActionPool *instance();

    virtual ~ActionPool();

    void init(PowerDevil::Core *parent);

    Action *loadAction(const QString &actionId, const KConfigGroup &group, PowerDevil::Core *parent);

    void unloadAllActiveActions();

    void clearCache();

private:
    ActionPool();

    QHash<QString, Action *> m_actionPool;
    QStringList m_activeActions;
};

}
