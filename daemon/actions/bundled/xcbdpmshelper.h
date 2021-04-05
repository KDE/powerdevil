/***************************************************************************
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

#ifndef XCBDPMSHELPER_H
#define XCBDPMSHELPER_H

#include "abstractdpmshelper.h"

#include <QScopedPointer>

namespace PowerDevil {
    class KWinKScreenHelperEffect;
}

class XcbDpmsHelper : public AbstractDpmsHelper
{
public:
    XcbDpmsHelper();
    ~XcbDpmsHelper() override;

    void trigger(const QString &type) override;
    void profileLoaded() override;
    void profileUnloaded() override;
    void startFade() override;
    void stopFade() override;
    void inhibited() override;
    void dpmsTimeout() override;

private:
    QScopedPointer<PowerDevil::KWinKScreenHelperEffect> m_fadeEffect;
};

#endif
