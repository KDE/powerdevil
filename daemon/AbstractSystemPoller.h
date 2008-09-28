/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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
 **************************************************************************/

#ifndef ABSTRACTSYSTEMPOLLER_H
#define ABSTRACTSYSTEMPOLLER_H

#include <QWidget>

class AbstractSystemPoller : public QWidget
{
    Q_OBJECT

public:

    enum PollingType {
        Abstract = -1,
        WidgetBased = 1,
        XSyncBased = 2,
        TimerBased = 3
    };

    AbstractSystemPoller(QObject *parent = 0);
    virtual ~AbstractSystemPoller();

    virtual PollingType getPollingType() = 0;
    virtual QString name() = 0;

    virtual bool isAvailable() = 0;
    virtual bool setUpPoller() = 0;
    virtual void unloadPoller() = 0;

public slots:
    virtual void setNextTimeout(int nextTimeout) = 0;
    virtual void forcePollRequest() = 0;
    virtual void stopCatchingTimeouts() = 0;
    virtual void catchIdleEvent() = 0;
    virtual void stopCatchingIdleEvents() = 0;
    virtual void simulateUserActivity() = 0;

signals:
    void resumingFromIdle();
    void pollRequest(int idleTime);

};

#endif /* ABSTRACTSYSTEMPOLLER_H */
