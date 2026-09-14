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

QDateTime DarkLightSchedule::nextMorning(const QDateTime &referenceDateTime) const
{
    auto nextMorning = m_schedule.nextTransition(referenceDateTime);
    if (nextMorning->type() == KDarkLightTransition::Morning) {
        return nextMorning->startDateTime();
    }

    // We are at an evening, get the next transition, which should be a morning.
    nextMorning = m_schedule.nextTransition(nextMorning->startDateTime());
    Q_ASSERT(nextMorning->type() == KDarkLightTransition::Morning);

    return nextMorning->startDateTime();
}

QDateTime DarkLightSchedule::nextEvening(const QDateTime &referenceDateTime) const
{
    auto nextEvening = m_schedule.nextTransition(referenceDateTime);
    if (nextEvening->type() == KDarkLightTransition::Evening) {
        return nextEvening->startDateTime();
    }

    // We are at a morning, get the next transition, which should be an evening.
    nextEvening = m_schedule.nextTransition(nextEvening->startDateTime());
    Q_ASSERT(nextEvening->type() == KDarkLightTransition::Evening);

    return nextEvening->startDateTime();
}

DarkLightScheduleProvider::DarkLightScheduleProvider(QObject *parent)
    : QObject(parent)
{
}

void DarkLightScheduleProvider::poll(QJSValue callback)
{
    KDarkLightScheduleProvider::poll().then(this, [this, callback](const KDarkLightSchedule &schedule) {
        const auto engine = qmlEngine(this);
        callback.call({engine->toScriptValue(DarkLightSchedule(schedule))});
    });
}

#include "moc_darklightschedule.cpp"
