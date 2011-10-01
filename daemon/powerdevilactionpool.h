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


#ifndef POWERDEVIL_POWERDEVILACTIONPOOL_H
#define POWERDEVIL_POWERDEVILACTIONPOOL_H

#include <QtCore/QHash>
#include <QtCore/QStringList>

class KConfigGroup;
namespace PowerDevil
{

class Core;
class Action;

class ActionPool
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

    QHash< QString, Action* > m_actionPool;
    QStringList m_activeActions;
};

}

#endif // POWERDEVIL_POWERDEVILACTIONPOOL_H
