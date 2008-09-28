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

#include "TimerBasedPoller.h"

#include <klocalizedstring.h>

TimerBasedPoller::TimerBasedPoller(QObject *parent)
        : AbstractSystemPoller(parent),
        m_pollTimer(new QTimer(this)),
        m_lastIdleTime(0),
        m_catchingIdle(false)
{

}

TimerBasedPoller::~TimerBasedPoller()
{

}

QString TimerBasedPoller::name()
{
    return i18n("Timer Based");
}

bool TimerBasedPoller::isAvailable()
{
#ifdef HAVE_XSCREENSAVER
    return true;
#else
    return false;
#endif
}

bool TimerBasedPoller::setUpPoller()
{
    m_pollTimer->start(800);

    connect(m_pollTimer, SIGNAL(timeout()), SLOT(poll()));

    return true;
}

void TimerBasedPoller::unloadPoller()
{
}

void TimerBasedPoller::setNextTimeout(int nextTimeout)
{
    Q_UNUSED(nextTimeout)
    // We don't need this
}

void TimerBasedPoller::forcePollRequest()
{
    poll();
}

///HACK yucky
#ifdef HAVE_XSCREENSAVER
#include <X11/extensions/scrnsaver.h>
#include <QX11Info>
#endif

void TimerBasedPoller::poll()
{
    /* Hack! Since KRunner still doesn't behave properly, the
     * correct way to go doesn't work (yet), and it's this one:
            ------------------------------------------------------------
            int idle = m_screenSaverIface->GetSessionIdleTime();
            ------------------------------------------------------------
     */
    /// In the meanwhile, this X11 hackish way gets its job done.
    //----------------------------------------------------------

    int idle = 0;

#ifdef HAVE_XSCREENSAVER
    XScreenSaverInfo * mitInfo = 0;
    mitInfo = XScreenSaverAllocInfo();
    XScreenSaverQueryInfo(QX11Info::display(), DefaultRootWindow(QX11Info::display()), mitInfo);
    idle = mitInfo->idle / 1000;
    //----------------------------------------------------------
#endif

    if (m_lastIdleTime > idle && m_catchingIdle) {
        // Hey, a wakeup!
        emit resumingFromIdle();
    } else {
        // Let's poll
        m_lastIdleTime = idle;
        emit pollRequest(m_lastIdleTime);
    }
}

void TimerBasedPoller::stopCatchingTimeouts()
{
    // Nothing we can do here
}

void TimerBasedPoller::stopCatchingIdleEvents()
{
    m_catchingIdle = false;
}

void TimerBasedPoller::catchIdleEvent()
{
    m_catchingIdle = true;
}

#include "TimerBasedPoller.moc"
