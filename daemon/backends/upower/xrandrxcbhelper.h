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

#ifndef XRANDR_XCB_HELPER_H
#define XRANDR_XCB_HELPER_H

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <QX11Info>
#include <QObject>
#include <QAbstractNativeEventFilter>

class XRandRInfo
{
public:
    int version;
    int eventBase;
    int errorBase;
    int majorOpcode;
    int eventType;
    xcb_atom_t backlightAtom;
    bool isPresent;
};

class XRandRXCBHelper : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    static inline XRandRXCBHelper* self()
    {
        static XRandRXCBHelper* s_instance = nullptr;
        if (!s_instance) {
            s_instance = new XRandRXCBHelper();
            if (!s_instance->isValid()) {
                s_instance = nullptr;
            }
        }

        return s_instance;
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, long int* result) override;

Q_SIGNALS:
        void brightnessChanged();

private:
    XRandRXCBHelper();
    ~XRandRXCBHelper() override;
    void init();

    inline bool isValid()
    {
        return s_xrandrInfo.isPresent;
    }

    inline xcb_connection_t *conn()
    {
        static xcb_connection_t *s_con = nullptr;
        if (!s_con) {
            s_con = QX11Info::connection();
        }
        return s_con;
    }

    inline xcb_window_t rootWindow(xcb_connection_t *c, int screen)
    {
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(c));
        for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(c));
                it.rem;
                --screen, xcb_screen_next(&it)) {
            if (screen == 0) {
                return iter.data->root;
            }
        }
        return XCB_WINDOW_NONE;
    }

    uint32_t m_window;
    static bool s_init;
    static XRandRInfo s_xrandrInfo;
};

#endif //XRANDR_XCB_HELPER_H