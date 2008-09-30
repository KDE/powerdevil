/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
 *   Copyright (C) 2008 by Kevin Ottens <ervin@kde.org>                    *
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

#include "SuspensionLockHandler.h"

#include <KDebug>
#include <klocalizedstring.h>

SuspensionLockHandler::SuspensionLockHandler(QObject *parent)
        : QObject(parent),
        m_isJobOngoing(false),
        m_isOnNotification(false),
        m_latestInhibitCookie(0)
{
}

SuspensionLockHandler::~SuspensionLockHandler()
{
}

bool SuspensionLockHandler::canStartSuspension()
{
    if (hasInhibit()) {
        return false;
    }

    return !m_isJobOngoing;
}

bool SuspensionLockHandler::canStartNotification()
{
    if (hasInhibit()) {
        return false;
    }

    if (!m_isJobOngoing && !m_isOnNotification) {
        return true;
    } else {
        return false;
    }
}

bool SuspensionLockHandler::hasInhibit()
{
    if (m_inhibitRequests.isEmpty()) {
        return false;
    } else {
        kDebug() << "Inhibition detected!!";
        // TODO: uhm... maybe a better notification here?
        emit streamCriticalNotification("inhibition", i18n("The application %1 "
                                        "is inhibiting suspension for the following reason:\n%2",
                                        m_inhibitRequests[m_latestInhibitCookie].application,
                                        m_inhibitRequests[m_latestInhibitCookie].reason),
                                        0, "dialog-cancel");
        return true;
    }
}

bool SuspensionLockHandler::setNotificationLock()
{
    if (!canStartNotification()) {
        kDebug() << "Notification lock present, aborting";
        return false;
    }

    m_isOnNotification = true;

    return true;
}

bool SuspensionLockHandler::setJobLock()
{
    if (!canStartSuspension()) {
        kDebug() << "Suspension lock present, aborting";
        return false;
    }

    m_isJobOngoing = true;

    return true;
}

int SuspensionLockHandler::inhibit(const QString &application, const QString &reason)
{
    m_latestInhibitCookie++;

    InhibitRequest req;
    //TODO: Keep track of the service name too, to cleanup cookie in case of a crash.
    req.application = application;
    req.reason = reason;
    m_inhibitRequests[m_latestInhibitCookie] = req;

    return m_latestInhibitCookie;
}

void SuspensionLockHandler::releaseNotificationLock()
{
    kDebug() << "Releasing notification lock";
    m_isOnNotification = false;
}

void SuspensionLockHandler::releaseAllLocks()
{
    kDebug() << "Releasing locks";
    m_isJobOngoing = false;
    m_isOnNotification = false;
}

void SuspensionLockHandler::releaseAllInhibitions()
{
    m_inhibitRequests.clear();
}

void SuspensionLockHandler::releaseInhibiton(int cookie)
{
    m_inhibitRequests.remove(cookie);
}

#include "SuspensionLockHandler.moc"
