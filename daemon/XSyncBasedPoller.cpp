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

#include "XSyncBasedPoller.h"

#include <QX11Info>

#include <klocalizedstring.h>

XSyncBasedPoller::XSyncBasedPoller( QObject *parent )
: AbstractSystemPoller( parent ),
m_display(QX11Info::display()),
m_idleCounter(None),
m_timeoutAlarm(None),
m_resetAlarm(None)
{
}

XSyncBasedPoller::~XSyncBasedPoller()
{
}

QString XSyncBasedPoller::name()
{
    return i18n("XSync Based (recommended)");
}

bool XSyncBasedPoller::isAvailable()
{
    if (!XSyncQueryExtension (m_display, &m_sync_event, &m_sync_error))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool XSyncBasedPoller::setUpPoller()
{
    int ncounters;
    int sync_major, sync_minor;

    if (!isAvailable())
    {
        return false;
    }

    if ( !XSyncInitialize (m_display, &sync_major, &sync_minor) )
    {
        return false;
    }

    m_counters = XSyncListSystemCounters (m_display, &ncounters);

    for (int i = 0; i < ncounters && !m_idleCounter; i++)
    {
        if (!strcmp(m_counters[i].name, "IDLETIME"))
            m_idleCounter = m_counters[i].counter;
    }

    if (!m_idleCounter)
    {
        return false;
    }

    return true;
}

void XSyncBasedPoller::unloadPoller()
{
    XSyncFreeSystemCounterList(m_counters);
}

void XSyncBasedPoller::setNextTimeout(int nextTimeout)
{
    XSyncValue timeout;

    XSyncIntToValue (&timeout, nextTimeout);
    setAlarm (m_display, &m_timeoutAlarm, m_idleCounter,
            XSyncPositiveComparison, timeout);
}

void XSyncBasedPoller::forcePollRequest()
{
    poll();
}

void XSyncBasedPoller::poll()
{
    XSyncValue idleTime;

    XSyncQueryCounter(m_display, m_idleCounter, &idleTime);

    emit pollRequest(XSyncValueHigh32(idleTime));
}

void XSyncBasedPoller::stopCatchingTimeouts()
{
    // We simply set the counter to an high value here.

    XSyncValue timeout;

    XSyncIntToValue (&timeout, 10000000);
    setAlarm (m_display, &m_timeoutAlarm, m_idleCounter,
            XSyncPositiveComparison, timeout);
}

void XSyncBasedPoller::stopCatchingIdleEvents()
{
    // FIXME: How to stop the timer?

    /*setAlarm (dpy, &resetAlarm, idleCounter,
                          XSyncNegativeComparison, alarmEvent->counter_value);*/
}

void XSyncBasedPoller::catchIdleEvent()
{
    XSyncValue idleTime;

    XSyncQueryCounter(m_display, m_idleCounter, &idleTime);

    setAlarm (m_display, &m_resetAlarm, m_idleCounter,
            XSyncNegativeComparison, idleTime);
}

bool XSyncBasedPoller::catchX11Event(XEvent *event)
{
    XSyncAlarmNotifyEvent *alarmEvent;

    //XNextEvent (m_display, event);

    if (event->type != m_sync_event + XSyncAlarmNotify)
    {
        return false;
    }

    alarmEvent = (XSyncAlarmNotifyEvent *)event;

    if (alarmEvent->alarm == m_timeoutAlarm)
    {
        /* Bling! Catched! */
        poll();
        return false;
    }
    else if (alarmEvent->alarm == m_resetAlarm)
    {
        /* Resuming from idle here! */
        emit resumingFromIdle();
    }

    return false;
}

void XSyncBasedPoller::setAlarm(Display *dpy, XSyncAlarm *alarm, XSyncCounter counter,
          XSyncTestType test, XSyncValue value)
{
    XSyncAlarmAttributes  attr;
    XSyncValue            delta;
    unsigned int          flags;

    XSyncIntToValue (&delta, 0);

    attr.trigger.counter     = counter;
    attr.trigger.value_type  = XSyncAbsolute;
    attr.trigger.test_type   = test;
    attr.trigger.wait_value  = value;
    attr.delta               = delta;

    flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
    XSyncCAValue | XSyncCADelta;

    if (*alarm)
        XSyncChangeAlarm (dpy, *alarm, flags, &attr);
    else
        *alarm = XSyncCreateAlarm (dpy, flags, &attr);
}


#include "XSyncBasedPoller.moc"
