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

#ifndef POLLSYSTEMLOADER_H
#define POLLSYSTEMLOADER_H

#include <QObject>
#include <QMap>
#include <QPointer>

#include "AbstractSystemPoller.h"

class PollSystemLoader : public QObject
{
        Q_OBJECT

    public:
        PollSystemLoader( QObject *parent = 0 );
        virtual ~PollSystemLoader();

        QMap<AbstractSystemPoller::PollingType, QString> getAvailableSystems();
        QMap<int, QString> getAvailableSystemsAsInt();

        bool loadSystem( AbstractSystemPoller::PollingType type );
        bool unloadCurrentSystem();

        AbstractSystemPoller *poller() {
            return m_poller;
        };

        void createAvailableCache();

    private:
        QPointer<AbstractSystemPoller> m_poller;
        QMap<AbstractSystemPoller::PollingType, QString> m_availableSystems;
};

#endif /* POLLSYSTEMLOADER_H */
