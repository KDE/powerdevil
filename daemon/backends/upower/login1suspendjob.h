/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2013 Lukáš Tinkl <ltinkl@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <QDBusInterface>
#include <QDBusPendingCallWatcher>

#include <KJob>

#include "powerdevilbackendinterface.h"

class Login1SuspendJob : public KJob
{
    Q_OBJECT
public:
    Login1SuspendJob(QDBusInterface *login1Interface,
                     PowerDevil::BackendInterface::SuspendMethod method,
                     PowerDevil::BackendInterface::SuspendMethods supported);
    ~Login1SuspendJob() override;

    void start() override;

private Q_SLOTS:
    void doStart();
    void sendResult(QDBusPendingCallWatcher *watcher);
    void slotLogin1Resuming(bool active);

private:
    QDBusInterface *m_login1Interface;
    PowerDevil::BackendInterface::SuspendMethod m_method;
    PowerDevil::BackendInterface::SuspendMethods m_supported;
};
