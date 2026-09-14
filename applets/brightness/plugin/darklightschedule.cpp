/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "darklightschedule.h"

#include <QQmlEngine>

DarkLightSchedule::DarkLightSchedule()
{
}

DarkLightSchedule::DarkLightSchedule(const KDarkLightSchedule &schedule)
    : m_schedule(schedule)
{
}

QDateTime DarkLightSchedule::nextTransition(const QDateTime &dateTime) const
{
    auto nextMorning = m_schedule.nextTransition(dateTime);
    return nextMorning->endDateTime();
}

DarkLightScheduleProvider::DarkLightScheduleProvider(QObject *parent)
    : QObject(parent)
{
}

void DarkLightScheduleProvider::poll(QJSValue callback)
{
    KDarkLightScheduleProvider::poll().then(this, [this, callback](const KDarkLightSchedule &schedule) {
        auto engine = qmlEngine(this);
        callback.call({engine->toScriptValue(DarkLightSchedule(schedule))});
    });
}

#include "moc_darklightschedule.cpp"
