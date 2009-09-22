/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kde.org>                      *
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

#ifndef SUSPENSIONLOCKHANDLER_H
#define SUSPENSIONLOCKHANDLER_H

#include <QObject>
#include <QMap>


class InhibitRequest
{
public:
    QString application;
    QString reason;
    int cookie;
};

class SuspensionLockHandler : public QObject
{
    Q_OBJECT

public:
    SuspensionLockHandler(QObject *parent = 0);
    virtual ~SuspensionLockHandler();

public slots:
    bool canStartSuspension(bool automated = false);
    bool canStartNotification(bool automated = false);

    bool hasInhibit(bool notify = false);

    bool setNotificationLock(bool automated = false);
    bool setJobLock(bool automated = false);

    int inhibit(const QString &application, const QString &reason);
    int inhibit(const QString &application, const QString &reason, int timeout);

    void releaseAllLocks();
    void releaseNotificationLock();
    void releaseAllInhibitions();
    void releaseInhibiton(int cookie);

private slots:
    void inhibitionTimeout();

signals:
    void streamCriticalNotification(const QString &evid, const QString &message,
                                    const char *slot, const QString &iconname);
    void inhibitChanged(bool changed);

private:
    bool m_isJobOngoing;
    bool m_isOnNotification;

    int m_latestInhibitCookie;
    QMap<int, InhibitRequest> m_inhibitRequests;
};

#endif /* SUSPENSIONLOCKHANDLER_H */
