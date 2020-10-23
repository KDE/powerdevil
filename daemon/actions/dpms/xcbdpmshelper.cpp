/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
 *   Copyright (C) 2015 by Martin Gräßlin <mgraesslin@kde.org>             *
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
#include "xcbdpmshelper.h"
// powerdevil
#include <powerdevilpolicyagent.h>
#include <powerdevil_debug.h>

#include <kwinkscreenhelpereffect.h>

// Qt
#include <QX11Info>

// xcb
#include <xcb/dpms.h>

template <typename T> using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

XcbDpmsHelper::XcbDpmsHelper()
    : AbstractDpmsHelper()
    , m_fadeEffect(new PowerDevil::KWinKScreenHelperEffect())
{
    auto *c = QX11Info::connection();

    xcb_prefetch_extension_data(c, &xcb_dpms_id);
    auto *extension = xcb_get_extension_data(c, &xcb_dpms_id);
    if (!extension || !extension->present) {
        qCWarning(POWERDEVIL) << "DPMS extension not available";
        return;
    }

    ScopedCPointer<xcb_dpms_capable_reply_t> capableReply(xcb_dpms_capable_reply(c,
        xcb_dpms_capable(c),
    nullptr));

    if (capableReply && capableReply->capable) {
        setSupported(true);
    }
}

XcbDpmsHelper::~XcbDpmsHelper() = default;

void XcbDpmsHelper::profileLoaded()
{
    // Disable a default timeout, if any
    xcb_dpms_set_timeouts(QX11Info::connection(), 0, 0, 0);
}

void XcbDpmsHelper::profileUnloaded()
{
    using namespace PowerDevil;

    if (!(PolicyAgent::instance()->unavailablePolicies() & PolicyAgent::ChangeScreenSettings)) {
        xcb_dpms_disable(QX11Info::connection());
    } else {
        qCDebug(POWERDEVIL) << "Not performing DPMS action due to inhibition";
    }
}

void XcbDpmsHelper::startFade()
{
    m_fadeEffect->start();
}

void XcbDpmsHelper::stopFade()
{
    m_fadeEffect->stop();
}

void XcbDpmsHelper::trigger(const QString &type)
{
    auto *c = QX11Info::connection();

    ScopedCPointer<xcb_dpms_info_reply_t> infoReply(xcb_dpms_info_reply(c,
        xcb_dpms_info(c),
    nullptr));

    if (!infoReply) {
        qCWarning(POWERDEVIL) << "Failed to query DPMS state, cannot trigger";
        return;
    }
    int level = 0;

    if (type == QLatin1String("ToggleOnOff")) {
        if (infoReply->power_level < XCB_DPMS_DPMS_MODE_OFF) {
            level = XCB_DPMS_DPMS_MODE_OFF;
        } else {
            level = XCB_DPMS_DPMS_MODE_ON;
        }
    } else if (type == QLatin1String("TurnOff")) {
        level = XCB_DPMS_DPMS_MODE_OFF;
    } else if (type == QLatin1String("Standby")) {
        level = XCB_DPMS_DPMS_MODE_STANDBY;
    } else if (type == QLatin1String("Suspend")) {
        level = XCB_DPMS_DPMS_MODE_SUSPEND;
    } else {
        // this leaves DPMS enabled but if it's meant to be disabled
        // then the timeouts will be zero and so effectively disabled
        return;
    }

    if (!infoReply->state) {
        xcb_dpms_enable(c);
    }

    xcb_dpms_force_level(c, level);
}

void XcbDpmsHelper::inhibited()
{
    qCDebug(POWERDEVIL) << "Disabling DPMS due to inhibition";
    xcb_dpms_disable(QX11Info::connection()); // wakes the screen - do we want this?
}

void XcbDpmsHelper::dpmsTimeout()
{
    trigger(QStringLiteral("TurnOff"));
}
