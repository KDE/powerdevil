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


#ifndef POWERDEVIL_POWERDEVILPOLICYAGENT_H
#define POWERDEVIL_POWERDEVILPOLICYAGENT_H

#include <QtCore/QObject>

class QDBusInterface;

namespace PowerDevil
{

class PolicyAgent : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PolicyAgent)

public:
    static PolicyAgent *instance();

    virtual ~PolicyAgent();

    bool canInterruptSession();
    bool canChangeProfile();
    bool canChangeScreenSettings();

private:
    explicit PolicyAgent(QObject* parent = 0);

    void init();
    void startSessionInterruption();
    void finishSessionInterruption();

    bool m_ckAvailable;
    QDBusInterface *m_ckSessionInterface;
    bool m_sessionIsBeingInterrupted;

    friend class Core;
};

}

#endif // POWERDEVIL_POWERDEVILPOLICYAGENT_H
