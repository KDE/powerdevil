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

#include "WidgetBasedPoller.h"

#include <QWidget>
#include <QTimer>
#include <QX11Info>
#include <QEvent>

#include <klocalizedstring.h>

WidgetBasedPoller::WidgetBasedPoller(QObject *parent)
        : AbstractSystemPoller(parent)
{
}

WidgetBasedPoller::~WidgetBasedPoller()
{
}

QString WidgetBasedPoller::name()
{
    return i18n("Grabber Widget Based");
}

bool WidgetBasedPoller::isAvailable()
{
#ifdef HAVE_XSCREENSAVER
    return true;
#else
    return false;
#endif
}

bool WidgetBasedPoller::setUpPoller()
{
    m_pollTimer = new QTimer(this);

    //setup idle timer, with some smart polling
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(poll()));

    // This code was taken from Lithium/KDE4Powersave
    m_grabber = new QWidget(0, Qt::X11BypassWindowManagerHint);
    m_grabber->move(-1000, -1000);
    m_grabber->setMouseTracking(true);
    m_grabber->installEventFilter(this);
    m_grabber->setObjectName("PowerDevilGrabberWidget");

    m_screenSaverIface = new OrgFreedesktopScreenSaverInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
            QDBusConnection::sessionBus(), this);

    connect(m_screenSaverIface, SIGNAL(ActiveChanged(bool)), SLOT(screensaverActivated(bool)));

    return true;
}

void WidgetBasedPoller::unloadPoller()
{
    m_pollTimer->deleteLater();
    m_grabber->deleteLater();
}

void WidgetBasedPoller::setNextTimeout(int nextTimeout)
{
    m_pollTimer->start(nextTimeout);
}

void WidgetBasedPoller::screensaverActivated(bool activated)
{
    // We care only if it has been disactivated

    if (!activated) {
        m_screenSaverIface->SimulateUserActivity();
        emit resumingFromIdle();
    }
}

bool WidgetBasedPoller::eventFilter(QObject * object, QEvent * event)
{
    if (object == m_grabber
            && (event->type() == QEvent::MouseMove || event->type() == QEvent::KeyPress)) {
        detectedActivity();
    } else if (object != m_grabber) {
        // If it's not the grabber, fallback to default event filter
        return false;
    }

    // Otherwise, simply ignore it
    return false;

}

void WidgetBasedPoller::waitForActivity()
{
    // This code was taken from Lithium/KDE4Powersave

    //emit pollEvent( I18N_NOOP( "Grabber widget is on" ) );

    m_grabber->show();
    m_grabber->grabMouse();
    m_grabber->grabKeyboard();

}

void WidgetBasedPoller::detectedActivity()
{
    //emit pollEvent( i18n( "Detected Activity" ) );

    emit resumingFromIdle();
}

void WidgetBasedPoller::releaseInputLock()
{
    //emit pollEvent( I18N_NOOP( "Grabber widget is off" ) );

    m_grabber->releaseMouse();
    m_grabber->releaseKeyboard();
    m_grabber->hide();
}

#ifdef HAVE_XSCREENSAVER
#include <X11/extensions/scrnsaver.h>
#endif

void WidgetBasedPoller::poll()
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

    emit pollRequest(idle);
}

void WidgetBasedPoller::forcePollRequest()
{
    poll();
}

void WidgetBasedPoller::stopCatchingTimeouts()
{
    m_pollTimer->stop();
}

void WidgetBasedPoller::catchIdleEvent()
{
    waitForActivity();
}

void WidgetBasedPoller::stopCatchingIdleEvents()
{
    releaseInputLock();
}

#include "WidgetBasedPoller.moc"
