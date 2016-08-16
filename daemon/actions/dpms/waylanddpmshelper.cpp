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
#include "waylanddpmshelper.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/registry.h>

#include <QVector>

using namespace KWayland::Client;

WaylandDpmsHelper::WaylandDpmsHelper()
    : QObject()
    , AbstractDpmsHelper()
    , m_connection()
{
    init();
}

WaylandDpmsHelper::~WaylandDpmsHelper() = default;

void WaylandDpmsHelper::trigger(const QString &type)
{
    Dpms::Mode level = Dpms::Mode::On;
    if (type == QLatin1String("ToggleOnOff")) {
        for (auto it = m_dpms.constBegin(); it != m_dpms.constEnd(); ++it) {
            auto dpms = it.value();
            if (!dpms) {
                continue;
            }
            if (dpms->mode() == Dpms::Mode::On) {
                dpms->requestMode(Dpms::Mode::Off);
            } else {
                dpms->requestMode(Dpms::Mode::On);
            }
        }
        m_connection->flush();
        return;
    } else if (type == QLatin1String("TurnOff")) {
        level = Dpms::Mode::Off;
    } else if (type == QLatin1String("Standby")) {
        level = Dpms::Mode::Standby;
    } else if (type == QLatin1String("Suspend")) {
        level = Dpms::Mode::Suspend;
    } else {
        level = Dpms::Mode::On;
    }
    requestMode(level);
}

void WaylandDpmsHelper::dpmsTimeout()
{
    requestMode(Dpms::Mode::Off);
}

void WaylandDpmsHelper::requestMode(Dpms::Mode mode)
{
    for (auto it = m_dpms.constBegin(); it != m_dpms.constEnd(); ++it) {
        auto dpms = it.value();
        if (!dpms) {
            continue;
        }
        dpms->requestMode(mode);
    }
    m_connection->flush();
}

void WaylandDpmsHelper::init()
{
    m_connection = ConnectionThread::fromApplication(this);
    if (!m_connection) {
        return;
    }
    m_registry = new Registry(m_connection);
    connect(m_registry, &Registry::dpmsAnnounced, this, [this] { setSupported(true);}, Qt::DirectConnection);
    connect(m_registry, &Registry::interfacesAnnounced, this, &WaylandDpmsHelper::initWithRegistry, Qt::QueuedConnection);
    m_registry->create(m_connection);
    m_registry->setup();

    m_connection->roundtrip();
}

void WaylandDpmsHelper::initWithRegistry()
{
    const auto dpmsData = m_registry->interface(Registry::Interface::Dpms);
    if (dpmsData.name != 0) {
        m_dpmsManager = m_registry->createDpmsManager(dpmsData.name, dpmsData.version, this);
    }
    // TODO: support a later announcement of dpmsManager
    connect(m_registry, &Registry::outputAnnounced, this, &WaylandDpmsHelper::initOutput);
    const auto outputs = m_registry->interfaces(Registry::Interface::Output);
    for (auto o: outputs) {
        initOutput(o.name, o.version);
    }
}

void WaylandDpmsHelper::initOutput(quint32 name, quint32 version)
{
    auto output = m_registry->createOutput(name, version, this);
    connect(output, &Output::removed, this,
        [this, output] {
            auto it = m_dpms.find(output);
            if (it == m_dpms.end()) {
                return;
            }
            auto dpms = it.value();
            m_dpms.erase(it);
            if (dpms) {
                dpms->deleteLater();
            }
            output->deleteLater();
        }, Qt::QueuedConnection
    );
    Dpms *dpms = nullptr;
    if (m_dpmsManager) {
        dpms = m_dpmsManager->getDpms(output, this);
        connect(dpms, &Dpms::modeChanged, this,
            [this, dpms] {
                Dpms::Mode type = dpms->mode();
                if (type == Dpms::Mode::On) {
                    if (m_oldScreenBrightness > 0) {
                        backend()->setBrightness(m_oldScreenBrightness, PowerDevil::BackendInterface::Screen);
                        m_oldScreenBrightness = 0;
                    }
                } else {
                    m_oldScreenBrightness = backend()->brightness(PowerDevil::BackendInterface::Screen);
                    backend()->setBrightness(0, PowerDevil::BackendInterface::Screen);
                }
            }, Qt::QueuedConnection
        );
    }
    m_dpms.insert(output, dpms);
}
