/***************************************************************************
 *   Copyright (C) 2015 Kai Uwe Broulik <kde@privat.broulik.de>            *
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

#include "kwinkscreenhelpereffect.h"

#include <QCoreApplication>
#include <QX11Info>

namespace PowerDevil
{

KWinKScreenHelperEffect::KWinKScreenHelperEffect(QObject *parent) : QObject(parent)
{
    m_abortTimer.setSingleShot(true);
    m_abortTimer.setInterval(11000);
    connect(&m_abortTimer, &QTimer::timeout, this, &KWinKScreenHelperEffect::stop);

    qApp->installNativeEventFilter(this);
}

KWinKScreenHelperEffect::~KWinKScreenHelperEffect()
{
    stop();
}

bool KWinKScreenHelperEffect::start()
{
    m_isValid = checkValid();
    if (!m_isValid) {
        // emit fade out right away since the effect is not available
        Q_EMIT fadedOut();
        return false;
    }

    m_running = true;
    setEffectProperty(1);
    m_abortTimer.start();
    return true;
}

void KWinKScreenHelperEffect::stop()
{
    // Maybe somebody got confused, just reset the property then
    if (m_state == NormalState) {
        setEffectProperty(0);
    } else {
        setEffectProperty(3);
    }
    m_running = false;
    m_abortTimer.stop();
}

bool KWinKScreenHelperEffect::checkValid()
{
#ifdef HAVE_XCB
    if (QX11Info::isPlatformX11()) {
        QScopedPointer<xcb_list_properties_reply_t, QScopedPointerPodDeleter> propsReply(xcb_list_properties_reply(QX11Info::connection(),
            xcb_list_properties_unchecked(QX11Info::connection(), QX11Info::appRootWindow()),
        NULL));
        QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> atomReply(xcb_intern_atom_reply(QX11Info::connection(),
            xcb_intern_atom_unchecked(QX11Info::connection(), false, 25, "_KDE_KWIN_KSCREEN_SUPPORT"),
        NULL));

        if (propsReply.isNull() || atomReply.isNull()) {
            return false;
        }

        auto *atoms = xcb_list_properties_atoms(propsReply.data());
        for (int i = 0; i < propsReply->atoms_len; ++i) {
            if (atoms[i] == atomReply->atom) {
                m_atom = atomReply->atom;
                return true;
            }
        }

        m_atom = 0;
        return false;
    }
#endif
    return false;
}

void KWinKScreenHelperEffect::setEffectProperty(long value)
{
#ifdef HAVE_XCB
    if (m_isValid && QX11Info::isPlatformX11()) {
        xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, QX11Info::appRootWindow(), m_atom, XCB_ATOM_CARDINAL, 32, 1, &value);
    }
#else
    Q_UNUSED(value);
#endif
}

bool KWinKScreenHelperEffect::nativeEventFilter(const QByteArray &eventType, void *message, long int *result)
{
    Q_UNUSED(result);

    if (eventType != "xcb_generic_event_t") {
        return false;
    }

#ifdef HAVE_XCB
    if (m_isValid && QX11Info::isPlatformX11()) {
        auto e = static_cast<xcb_generic_event_t *>(message);
        const uint8_t type = e->response_type & ~0x80;
        if (type == XCB_PROPERTY_NOTIFY) {
            auto *event = reinterpret_cast<xcb_property_notify_event_t *>(e);
            if (event->window == QX11Info::appRootWindow()) {
                if (event->atom != m_atom) {
                    return false;
                }

                auto cookie = xcb_get_property(QX11Info::connection(), false, QX11Info::appRootWindow(), m_atom, XCB_ATOM_CARDINAL, 0, 1);
                QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> reply(xcb_get_property_reply(QX11Info::connection(), cookie, NULL));
                if (reply.isNull() || reply.data()->value_len != 1 || reply.data()->format != uint8_t(32)) {
                    return false;
                }

                auto *data = reinterpret_cast<uint32_t *>(xcb_get_property_value(reply.data()));
                if (!data) {
                    return false;
                }

                switch(*data) {
                case 1:
                    m_state = FadingOutState;
                    break;
                case 2:
                    m_state = FadedOutState;
                    if (m_running) {
                        Q_EMIT fadedOut();
                    }
                    break;
                case 3:
                    m_state = FadingInState;
                    m_running = false;
                    m_abortTimer.stop();
                    break;
                default:
                    m_state = NormalState;
                    m_running = false;
                }

                Q_EMIT stateChanged(m_state);
            }
        }
    }
#else
    Q_UNUSED(message);
#endif

    return false;
}

} // namespace PowerDevil
