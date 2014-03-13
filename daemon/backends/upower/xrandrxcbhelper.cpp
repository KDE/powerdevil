/*************************************************************************************
 *  Copyright (C) 2013 by Alejandro Fiestas Olivares <afiestas@kde.org>              *
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

#include "xrandrxcbhelper.h"

#include <xcb/randr.h>
#include <QCoreApplication>
#include <QLatin1String>

bool XRandRXCBHelper::s_init = false;
XRandRInfo XRandRXCBHelper::s_xrandrInfo;

XRandRXCBHelper::XRandRXCBHelper() : QObject()
{
    if (!s_init) {
        init();
    }
}

XRandRXCBHelper::~XRandRXCBHelper()
{
    xcb_destroy_window(conn(), m_window);
}

bool XRandRXCBHelper::nativeEventFilter(const QByteArray& eventType, void* message, long int* result)
{
    Q_UNUSED(result);

    if (eventType != "xcb_generic_event_t") {
        return false;
    }

    xcb_generic_event_t* e = static_cast<xcb_generic_event_t *>(message);
    const uint8_t xEventType = e->response_type & ~0x80;

    //If this event is not xcb_randr_notify, we don't want it
    if (xEventType != s_xrandrInfo.eventType) {
        return false;
    }

    xcb_randr_notify_event_t*
    randrEvent = reinterpret_cast<xcb_randr_notify_event_t*>(e);

    //If the event is not about a change in an output property, we don't want it
    if (randrEvent->subCode != XCB_RANDR_NOTIFY_OUTPUT_PROPERTY) {
        return false;
    }

    //If the event is not about a new value, we don't want it
    if (randrEvent->u.op.status != XCB_PROPERTY_NEW_VALUE) {
        return false;
    }

    //If the modified property is not backlight, we don't care
    if (!randrEvent->u.op.atom != s_xrandrInfo.backlightAtom) {
        return false;
    }

    Q_EMIT brightnessChanged();

    return false;
}

void XRandRXCBHelper::init()
{
    xcb_connection_t* c = conn();

    xcb_prefetch_extension_data(c, &xcb_randr_id);
    const xcb_query_extension_reply_t *reply = xcb_get_extension_data(c, &xcb_randr_id);
    if (!reply) {
        s_xrandrInfo.isPresent = false;
        return;
    }

    s_xrandrInfo.isPresent = reply->present;
    s_xrandrInfo.eventBase = reply->first_event;
    s_xrandrInfo.errorBase = reply->first_error;
    s_xrandrInfo.eventType = s_xrandrInfo.eventBase + XCB_RANDR_NOTIFY;
    s_xrandrInfo.majorOpcode = reply->major_opcode;

    QByteArray backlight("BackLight");

    /*This is KDE5... the world of opengl and wayland (almost), I don't think we really need to
    check the old BACKLIGHT atom*/
//     QByteArray backlightCaps("BACKLIGHT");
//     xcb_intern_atom(c, true, backlightCaps.length(), backlightCaps.constData());
    xcb_intern_atom_reply_t* atomReply =
    xcb_intern_atom_reply(c, xcb_intern_atom(c, true, backlight.length(), backlight.constData()), NULL);

    //If backlight atom doesn't exist, means that no driver is actually supporting it
    if (!atomReply) {
        return;
    }

    uint32_t rWindow = rootWindow(c, 0);
    m_window = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, m_window,
                      rWindow,
                      0, 0, 1, 1, 0, XCB_COPY_FROM_PARENT,
                      XCB_COPY_FROM_PARENT, 0, NULL);

    xcb_randr_select_input(c, m_window, XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);
    qApp->installNativeEventFilter(this);

    s_init = true;
}