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

XSyncBasedPoller::XSyncBasedPoller(QObject *parent)
        : AbstractSystemPoller(parent)
#ifdef HAVE_XSYNC
        , m_display(QX11Info::display())
        , m_idleCounter(None)
        , m_timeoutAlarm(None)
        , m_resetAlarm(None)
#endif
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
#ifdef HAVE_XSYNC
    if (!XSyncQueryExtension(m_display, &m_sync_event, &m_sync_error)) {
        return false;
    } else {
        return true;
    }
#else
    return false;
#endif
}

bool XSyncBasedPoller::setUpPoller()
{
#ifdef HAVE_XSYNC
    int ncounters;
    int sync_major, sync_minor;

    if (!isAvailable()) {
        return false;
    }

    if (!XSyncInitialize(m_display, &sync_major, &sync_minor)) {
        return false;
    }

    kDebug() << "XSync Inited";

    m_counters = XSyncListSystemCounters(m_display, &ncounters);

    bool idleFound = false;

    for (int i = 0; i < ncounters && !m_idleCounter; ++i) {
        if (!strcmp(m_counters[i].name, "IDLETIME")) {
            m_idleCounter = m_counters[i].counter;
            idleFound = true;
        }
    }

    if (!idleFound) {
        return false;
    }

    KApplication::kApplication()->installX11EventFilter(this);

    kDebug() << "Supported, init completed";

    return true;
#else
    return false;
#endif
}

void XSyncBasedPoller::unloadPoller()
{
    //XSyncFreeSystemCounterList( m_counters );
}

void XSyncBasedPoller::setNextTimeout(int nextTimeout)
{
    /* We need to set the counter to the idle time + the value
     * requested for next timeout
     */
#ifdef HAVE_XSYNC
    XSyncValue timeout;
    XSyncValue idleTime;
    XSyncValue result;
    int overflow;

    XSyncQueryCounter(m_display, m_idleCounter, &idleTime);

    XSyncIntToValue(&timeout, nextTimeout);

    XSyncValueAdd(&result, idleTime, timeout, &overflow);

    setAlarm(m_display, &m_timeoutAlarm, m_idleCounter,
             XSyncPositiveComparison, result);
#endif
}

void XSyncBasedPoller::forcePollRequest()
{
    poll();
}

void XSyncBasedPoller::poll()
{
#ifdef HAVE_XSYNC
    XSyncValue idleTime;

    XSyncQueryCounter(m_display, m_idleCounter, &idleTime);

    emit pollRequest(XSyncValueLow32(idleTime) / 1000);
#endif
}

void XSyncBasedPoller::stopCatchingTimeouts()
{
#ifdef HAVE_XSYNC
    XSyncDestroyAlarm(m_display, m_timeoutAlarm);
    m_timeoutAlarm = None;
#endif
}

void XSyncBasedPoller::stopCatchingIdleEvents()
{
#ifdef HAVE_XSYNC
    XSyncDestroyAlarm(m_display, m_resetAlarm);
    m_resetAlarm = None;
#endif
}

void XSyncBasedPoller::catchIdleEvent()
{
#ifdef HAVE_XSYNC
    XSyncValue idleTime;

    XSyncQueryCounter(m_display, m_idleCounter, &idleTime);

    /* Set the reset alarm to fire the next time idleCounter < the
     * current counter value. XSyncNegativeComparison means <= so
     * we have to subtract 1 from the counter value
     */

    int overflow;
    XSyncValue add;
    XSyncValue plusone;
    XSyncIntToValue(&add, -1);
    XSyncValueAdd(&plusone, idleTime, add, &overflow);
    setAlarm(m_display, &m_resetAlarm, m_idleCounter,
             XSyncNegativeComparison, plusone);
#endif

}

bool XSyncBasedPoller::x11Event(XEvent *event)
{
#ifdef HAVE_XSYNC
    XSyncAlarmNotifyEvent *alarmEvent;

    if (event->type != m_sync_event + XSyncAlarmNotify) {
        return false;
    }

    alarmEvent = (XSyncAlarmNotifyEvent *)event;

    if (alarmEvent->alarm == m_timeoutAlarm) {
        /* Bling! Caught! */
        emit pollRequest(XSyncValueLow32(alarmEvent->counter_value) / 1000);
        return false;
    } else if (alarmEvent->alarm == m_resetAlarm) {
        /* Resuming from idle here! */
        emit resumingFromIdle();
    }

    return false;
#else
    return false;
#endif
}

#ifdef HAVE_XSYNC
void XSyncBasedPoller::setAlarm(Display *dpy, XSyncAlarm *alarm, XSyncCounter counter,
                                XSyncTestType test, XSyncValue value)
{
    XSyncAlarmAttributes  attr;
    XSyncValue            delta;
    unsigned int          flags;

    XSyncIntToValue(&delta, 0);

    attr.trigger.counter     = counter;
    attr.trigger.value_type  = XSyncAbsolute;
    attr.trigger.test_type   = test;
    attr.trigger.wait_value  = value;
    attr.delta               = delta;

    flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
            XSyncCAValue | XSyncCADelta;

    if (*alarm)
        XSyncChangeAlarm(dpy, *alarm, flags, &attr);
    else
        *alarm = XSyncCreateAlarm(dpy, flags, &attr);
}
#endif

#include "XSyncBasedPoller.moc"
