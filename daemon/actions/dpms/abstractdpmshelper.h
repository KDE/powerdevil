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

#ifndef ABSTRACTDPMSHELPER_H
#define ABSTRACTDPMSHELPER_H

class QString;

class AbstractDpmsHelper
{
public:
    virtual ~AbstractDpmsHelper();

    virtual void startFade();
    virtual void stopFade();
    virtual void trigger(const QString &type) = 0;
    virtual void profileLoaded(int idleTime);
    virtual void profileUnloaded();
    virtual void inhibited();

    bool isSupported() const {
        return m_supported;
    }

protected:
    void setSupported(bool supported) {
        m_supported = supported;
    }

private:
    bool m_supported = false;
};

#endif
