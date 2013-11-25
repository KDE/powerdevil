/*************************************************************************************
 *  Copyright (C) 2013 by Dan Vrátil <dvratil@redhat.com>                            *
 *  Copyright (C) 2013 by Dario Freddi <drf@kde.org>                                 *
 *                                                                                   *
 *  This program is free software; you can redistribute it and/or                    *
 *  modify it under the terms of the GNU General Public License                      *
 *  as published by the Free Software Foundation; either version 2                   *
 *  of the License, or (at your option) any later version.                           *
 *                                                                                   *
 *  This program is distributed in the hope that it will be useful,                  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                    *
 *  GNU General Public License for more details.                                     *
 *                                                                                   *
 *  You should have received a copy of the GNU General Public License                *
 *  along with this program; if not, write to the Free Software                      *
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA   *
 *************************************************************************************/

#include "xrandrx11helper.h"

#include <QX11Info>

// #include <KSystemEventFilter>
#include <kdebug.h>

XRandRX11Helper::XRandRX11Helper():
    QWidget()
{
    XRRQueryVersion (QX11Info::display(), &m_versionMajor, &m_versionMinor);

    XRRQueryExtension(QX11Info::display(), &m_randrBase, &m_randrError);

    m_window = XCreateSimpleWindow(QX11Info::display(),
                                   XRootWindow(QX11Info::display(), DefaultScreen(QX11Info::display()))
                                   , 0, 0, 1, 1, 0, 0, 0 );

    XRRSelectInput(QX11Info::display(), m_window, RROutputPropertyNotifyMask);

#warning port to QAbstractNativeEventFilter
//     KSystemEventFilter::installEventFilter(this);
}

XRandRX11Helper::~XRandRX11Helper()
{
//     KSystemEventFilter::removeEventFilter(this);
    XDestroyWindow(QX11Info::display(), m_window);
}

bool XRandRX11Helper::x11Event(XEvent *event)
{
    XRRNotifyEvent* e2 = reinterpret_cast< XRRNotifyEvent* >(event);

    if (event->xany.type != m_randrBase + RRNotify) {
        return false;
    }

    if (e2->subtype == RRNotify_OutputProperty) {
        XRROutputPropertyNotifyEvent* e2 = reinterpret_cast< XRROutputPropertyNotifyEvent* >(event);
        char *atomName = XGetAtomName(QX11Info::display(), e2->property);
        if (QString(atomName) == RR_PROPERTY_BACKLIGHT) {
            Q_EMIT brightnessChanged();
        }

        XFree(atomName);
    }

    return false;
}

#include "xrandrx11helper.moc"
