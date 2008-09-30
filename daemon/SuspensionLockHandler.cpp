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
 ***************************************************************************/

#include "SuspensionLockHandler.h"

#include <KDebug>

SuspensionLockHandler::SuspensionLockHandler(QObject *parent)
        : QObject(parent),
        m_isJobOngoing(false),
        m_isOnNotification(false)
{
}

SuspensionLockHandler::~SuspensionLockHandler()
{
}

bool SuspensionLockHandler::canStartSuspension()
{
    return !m_isJobOngoing;
}

bool SuspensionLockHandler::canStartNotification()
{
    if (!m_isJobOngoing && !m_isOnNotification) {
        return true;
    } else {
        return false;
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

}

void SuspensionLockHandler::releaseInhibiton(const QString &id)
{

}

#include "SuspensionLockHandler.moc"
