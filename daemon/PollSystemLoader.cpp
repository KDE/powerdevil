/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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
 **************************************************************************/

#include "PollSystemLoader.h"

#include "WidgetBasedPoller.h"
#include "XSyncBasedPoller.h"
#include "TimerBasedPoller.h"

PollSystemLoader::PollSystemLoader(QObject *parent)
        : QObject(parent),
        m_poller(0)
{
    reloadAvailableCache();
}

PollSystemLoader::~PollSystemLoader()
{
    unloadCurrentSystem();
}

void PollSystemLoader::reloadAvailableCache()
{
    m_availableSystems.clear();

    // Test each polling system
    WidgetBasedPoller *wpl = new WidgetBasedPoller(this);
    XSyncBasedPoller *xpl = XSyncBasedPoller::instance();
    TimerBasedPoller *tpl = new TimerBasedPoller(this);

    if (wpl->isAvailable()) {
        m_availableSystems[AbstractSystemPoller::WidgetBased] = wpl->name();
    }
    if (xpl->isAvailable()) {
        m_availableSystems[AbstractSystemPoller::XSyncBased] = xpl->name();
    }
    if (tpl->isAvailable()) {
        m_availableSystems[AbstractSystemPoller::TimerBased] = tpl->name();
    }

    wpl->deleteLater();
    tpl->deleteLater();
}

QHash<AbstractSystemPoller::PollingType, QString> PollSystemLoader::getAvailableSystems()
{
    // Cached
    return m_availableSystems;
}

QHash<int, QString> PollSystemLoader::getAvailableSystemsAsInt()
{
    // Cached

    QHash<int, QString> retlist;

    QHash<AbstractSystemPoller::PollingType, QString>::const_iterator i;
    for (i = m_availableSystems.constBegin(); i != m_availableSystems.constEnd(); ++i) {
        retlist[(int) i.key()] = i.value();
    }

    return retlist;
}

bool PollSystemLoader::loadSystem(AbstractSystemPoller::PollingType type)
{
    if (m_poller) {
        if (m_poller->getPollingType() == type) {
            return true;
        } else {
            unloadCurrentSystem();
        }
    }

    switch (type) {
    case AbstractSystemPoller::WidgetBased:
        m_poller = new WidgetBasedPoller(this);
        break;
    case AbstractSystemPoller::TimerBased:
        m_poller = new TimerBasedPoller(this);
        break;
    case AbstractSystemPoller::XSyncBased:
        m_poller = XSyncBasedPoller::instance();
        break;
    default:
        return false;
        break;
    }

    if (!m_poller->isAvailable()) {
        return false;
    }

    if (!m_poller->setUpPoller()) {
        return false;
    }

    return true;
}

bool PollSystemLoader::unloadCurrentSystem()
{
    if (m_poller) {
        m_poller->unloadPoller();

        if (m_poller->getPollingType() != AbstractSystemPoller::XSyncBased) {
            m_poller->deleteLater();
        }
    }

    return true;
}

#include "PollSystemLoader.moc"
