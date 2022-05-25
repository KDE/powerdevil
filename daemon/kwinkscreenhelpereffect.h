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

#ifndef KWINKSCREENHELPEREFFECT_H
#define KWINKSCREENHELPEREFFECT_H

#include <config-powerdevil.h>

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>

#if HAVE_XCB
#include <xcb/xcb.h>
#endif

namespace PowerDevil
{
class Q_DECL_EXPORT KWinKScreenHelperEffect : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit KWinKScreenHelperEffect(QObject *parent = nullptr);
    ~KWinKScreenHelperEffect() override;

    enum State {
        NormalState,
        FadingOutState,
        FadedOutState,
        FadingInState
    };

    bool start();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

public Q_SLOTS:
    void stop();

Q_SIGNALS:
    void stateChanged(State state);
    void fadedOut();

private:
    bool checkValid();
    void setEffectProperty(long value);

    State m_state = NormalState;
    bool m_isValid = false;
    bool m_running = false;

    QTimer m_abortTimer;

#if HAVE_XCB
    xcb_atom_t m_atom = 0;
#endif

};

} // namespace PowerDevil

#endif // KWINKSCREENHELPEREFFECT_H
