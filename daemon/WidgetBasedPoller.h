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

#ifndef WIDGETBASEDPOLLER_H
#define WIDGETBASEDPOLLER_H

#include "AbstractSystemPoller.h"

#include <config-powerdevil.h>

#include "screensaver_interface.h"

class QTimer;
class QEvent;

class WidgetBasedPoller : public AbstractSystemPoller
{
        Q_OBJECT

    public:
        WidgetBasedPoller( QObject *parent = 0 );
        virtual ~WidgetBasedPoller();

        AbstractSystemPoller::PollingType getPollingType() {
            return AbstractSystemPoller::WidgetBased;
        };
        QString name();

        bool isAvailable();
        bool setUpPoller();
        void unloadPoller();

    protected:
        bool eventFilter( QObject * object, QEvent * event );

    public slots:
        void setNextTimeout( int nextTimeout );
        void forcePollRequest();
        void stopCatchingTimeouts();
        void catchIdleEvent();
        void stopCatchingIdleEvents();

    private slots:
        void poll();
        void detectedActivity();
        void waitForActivity();
        void releaseInputLock();
        void screensaverActivated( bool activated );

    signals:
        void resumingFromIdle();
        void pollRequest( int idleTime );

    private:
        QTimer * m_pollTimer;
        QWidget * m_grabber;

        OrgFreedesktopScreenSaverInterface * m_screenSaverIface;
};

#endif /* WIDGETBASEDPOLLER_H */
