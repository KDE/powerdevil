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

#ifndef WAYLANDDPMSHELPER_H
#define WAYLANDDPMSHELPER_H

#include "abstractdpmshelper.h"

#include <KWayland/Client/dpms.h>

#include <QObject>
#include <QMap>

namespace KWayland
{
namespace Client
{

class ConnectionThread;
class Registry;

}
}

class WaylandDpmsHelper : public QObject, public AbstractDpmsHelper
{
    Q_OBJECT

public:
    WaylandDpmsHelper();
    ~WaylandDpmsHelper() override;
    void trigger(const QString &type) override;
    void dpmsTimeout() override;

private:
    void init();
    void initWithRegistry();
    void initOutput(quint32 name, quint32 version);
    void requestMode(KWayland::Client::Dpms::Mode mode);

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::DpmsManager *m_dpmsManager = nullptr;
    QMap<KWayland::Client::Output*, KWayland::Client::Dpms*> m_dpms;

    int m_oldScreenBrightness;
};

#endif
