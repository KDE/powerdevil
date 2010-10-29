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


#include "powerdevildpmsaction.h"

#include <powerdevilcore.h>

#include <config-workspace.h>

#include <QtGui/QX11Info>
#include <KConfigGroup>
#include <KPluginFactory>

#include <X11/Xmd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
extern "C" {
#include <X11/extensions/dpms.h>
    int __kde_do_not_unload = 1;

#ifndef HAVE_DPMSCAPABLE_PROTO
    Bool DPMSCapable(Display *);
#endif

#ifndef HAVE_DPMSINFO_PROTO
    Status DPMSInfo(Display *, CARD16 *, BOOL *);
#endif

int dropError(Display *, XErrorEvent *);
typedef int (*XErrFunc)(Display *, XErrorEvent *);
}

int dropError(Display *, XErrorEvent *)
{
    return 0;
}

static XErrorHandler defaultHandler;

K_PLUGIN_FACTORY(PowerDevilDPMSActionFactory, registerPlugin<PowerDevilDPMSAction>(); )
K_EXPORT_PLUGIN(PowerDevilDPMSActionFactory("powerdevildpmsaction"))

PowerDevilDPMSAction::PowerDevilDPMSAction(QObject* parent, const QVariantList& )
    : Action(parent)
{

}

PowerDevilDPMSAction::~PowerDevilDPMSAction()
{

}

void PowerDevilDPMSAction::onProfileUnload()
{
    Display *dpy = QX11Info::display();
    if (m_hasDPMS) {
        DPMSDisable(dpy);
    }
}

void PowerDevilDPMSAction::onWakeupFromIdle()
{
    //
}

void PowerDevilDPMSAction::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void PowerDevilDPMSAction::onProfileLoad()
{
    Display *dpy = QX11Info::display();
    if (m_hasDPMS) {
        DPMSEnable(dpy);
    } else {
        return;
    }

    XFlush(dpy);
    XSetErrorHandler(defaultHandler);

    DPMSSetTimeouts(dpy, m_idleTime, (int)(m_idleTime * 1.5), m_idleTime * 2);

    XFlush(dpy);
    XSetErrorHandler(defaultHandler);
}

void PowerDevilDPMSAction::triggerImpl(const QVariantMap& args)
{
    CARD16 dummy;
    BOOL enabled;
    Display *dpy = QX11Info::display();
    DPMSInfo(dpy, &dummy, &enabled);

    // Let's pretend we're resuming
    core()->onResumeFromSuspend();

    if (args["Type"].toString() == "TurnOff") {
        if (enabled) {
            DPMSForceLevel(dpy, DPMSModeOff);
        } else {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, DPMSModeOff);
        }
    } else if (args["Type"].toString() == "Standby") {
        if (enabled) {
            DPMSForceLevel(dpy, DPMSModeStandby);
        } else {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, DPMSModeStandby);
        }
    } else if (args["Type"].toString() == "Suspend") {
        if (enabled) {
            DPMSForceLevel(dpy, DPMSModeSuspend);
        } else {
            DPMSEnable(dpy);
            DPMSForceLevel(dpy, DPMSModeSuspend);
        }
    }
}

bool PowerDevilDPMSAction::loadAction(const KConfigGroup& config)
{
    defaultHandler = XSetErrorHandler(dropError);

    Display *dpy = QX11Info::display();

    int dummy;
    m_hasDPMS = true;

    if (!DPMSQueryExtension(dpy, &dummy, &dummy) || !DPMSCapable(dpy)) {
        m_hasDPMS = false;
        XSetErrorHandler(defaultHandler);
    }

    if (config.hasKey("idleTime")) {
        m_idleTime = config.readEntry<int>("idleTime", 100000000);
    } else {
        m_hasDPMS = false;
    }

    return true;
}

#include "powerdevildpmsaction.moc"
