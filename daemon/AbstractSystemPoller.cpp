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

#include "AbstractSystemPoller.h"

#include <config-X11.h>

#ifdef HAVE_XTEST
#include <QX11Info>

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <fixx11h.h>
#endif // HAVE_XTEST

AbstractSystemPoller::AbstractSystemPoller(QObject *parent)
        : QWidget(0)
{
    Q_UNUSED(parent)
}

AbstractSystemPoller::~AbstractSystemPoller()
{
}

void AbstractSystemPoller::simulateUserActivity()
{
#ifdef HAVE_XTEST
    int XTestKeyCode = 0;
    Display* display = QX11Info::display();
    XTestFakeKeyEvent(display, XTestKeyCode, true, CurrentTime);
    XTestFakeKeyEvent(display, XTestKeyCode, false, CurrentTime);
    XSync(display, false);
#endif // HAVE_XTEST
}

#include "AbstractSystemPoller.moc"
