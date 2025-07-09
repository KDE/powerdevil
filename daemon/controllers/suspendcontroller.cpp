/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "suspendcontroller.h"

#include <powerdevil_debug.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include <QDir>
#include <QFile>

using namespace Qt::StringLiterals;

inline constexpr QLatin1StringView LOGIN1_SERVICE("org.freedesktop.login1");
inline constexpr QLatin1StringView CONSOLEKIT2_SERVICE("org.freedesktop.ConsoleKit");

inline static const QLatin1String s_wakeupSysFsPath("/sys/class/wakeup");

SuspendController::SuspendController()
    : QObject()
{
    // interfaces
    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(LOGIN1_SERVICE);
    }

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        // Activate it.
        QDBusConnection::systemBus().interface()->startService(CONSOLEKIT2_SERVICE);
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(LOGIN1_SERVICE)) {
        m_login1Interface =
            new QDBusInterface(LOGIN1_SERVICE, u"/org/freedesktop/login1"_s, u"org.freedesktop.login1.Manager"_s, QDBusConnection::systemBus(), this);
    }

    // if login1 isn't available, try using the same interface with ConsoleKit2
    if (!m_login1Interface && QDBusConnection::systemBus().interface()->isServiceRegistered(CONSOLEKIT2_SERVICE)) {
        m_login1Interface = new QDBusInterface(CONSOLEKIT2_SERVICE,
                                               u"/org/freedesktop/ConsoleKit/Manager"_s,
                                               u"org.freedesktop.ConsoleKit.Manager"_s,
                                               QDBusConnection::systemBus(),
                                               this);
    }

    // "resuming" signal
    if (m_login1Interface) {
        connect(m_login1Interface.data(), SIGNAL(PrepareForSleep(bool)), this, SLOT(slotLogin1PrepareForSleep(bool)));
    }
}

bool SuspendController::canSuspend() const
{
    return m_sessionManagement.canSuspend();
}

void SuspendController::suspend()
{
    m_sessionManagement.suspend();
}

bool SuspendController::canHibernate() const
{
    return m_sessionManagement.canHibernate();
}

void SuspendController::hibernate()
{
    m_sessionManagement.hibernate();
}

bool SuspendController::canHybridSuspend() const
{
    return m_sessionManagement.canHybridSuspend();
}

void SuspendController::hybridSuspend()
{
    m_sessionManagement.hybridSuspend();
}

bool SuspendController::canSuspendThenHibernate() const
{
    return m_sessionManagement.canSuspendThenHibernate();
}

void SuspendController::suspendThenHibernate()
{
    m_sessionManagement.suspendThenHibernate();
}

void SuspendController::slotLogin1PrepareForSleep(bool active)
{
#ifdef Q_OS_LINUX
    snapshotWakeupCounts(active);
#endif

    if (active) {
        Q_EMIT aboutToSuspend();
    } else {
        Q_EMIT resumeFromSuspend();
    }
}

#ifdef Q_OS_LINUX

void SuspendController::snapshotWakeupCounts(bool active)
{
    // TODO: does this need to be list?
    QStringList changed;

    // clear out previously recorded values
    if (active) {
        m_wakeupCounts.clear();
    }

    // read all wakeups, if we are waking up, check against previous values
    const QStringList wakeups = QDir(s_wakeupSysFsPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &wakeup : wakeups) {
        QDir wakeupDir(QString(s_wakeupSysFsPath + u'/' + wakeup));

        // read wakeup source name
        QFile file(wakeupDir.absoluteFilePath(u"name"_s));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString name;
        QTextStream stream(&file);
        stream >> name;

        // read wakeup count
        QFile countFile(wakeupDir.absoluteFilePath(u"wakeup_count"_s));
        int count = 0;
        if (countFile.open(QIODevice::ReadOnly)) {
            count = countFile.readAll().trimmed().toInt();
        }

        if (active) {
            m_wakeupCounts.insert(name, count);
        } else {
            if (m_wakeupCounts.contains(name) && (m_wakeupCounts.value(name) < count)) {
                qCDebug(POWERDEVIL) << "Wakeup source" << name << "resumed system from suspend";
                changed << name;
            }
        }
    }
    qDebug() << "Changed" << changed;
}

#endif

#include "moc_suspendcontroller.cpp"
