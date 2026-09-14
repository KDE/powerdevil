/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "daynightschedule.h"

DayNightTransition::DayNightTransition(QObject *parent)
    : QObject(parent)
{
}

QDateTime DayNightTransition::startDateTime() const
{
    return m_startDateTime;
}

void DayNightTransition::setStartDateTime(const QDateTime &dateTime)
{
    if (m_startDateTime != dateTime) {
        m_startDateTime = dateTime;
        Q_EMIT startDateTimeChanged();
    }
}

QDateTime DayNightTransition::endDateTime() const
{
    return m_endDateTime;
}

void DayNightTransition::setEndDateTime(const QDateTime &dateTime)
{
    if (m_endDateTime != dateTime) {
        m_endDateTime = dateTime;
        Q_EMIT endDateTimeChanged();
    }
}

DayNightSchedule::DayNightSchedule(QObject *parent)
    : QObject(parent)
    , m_provider(std::make_unique<KDarkLightScheduleProvider>())
    , m_nextMorning(std::make_unique<DayNightTransition>())
    , m_nextEvening(std::make_unique<DayNightTransition>())
{
    update();
    connect(m_provider.get(), &KDarkLightScheduleProvider::scheduleChanged, this, &DayNightSchedule::update);
}

DayNightSchedule::~DayNightSchedule()
{
}

void DayNightSchedule::update()
{
    const auto schedule = m_provider->schedule();
    const auto now = QDateTime::currentDateTime();

    auto nextMorning = schedule.nextTransition(now);
    auto nextEvening = schedule.nextTransition(nextMorning->endDateTime());
    if (nextMorning->type() != KDarkLightTransition::Morning) {
        std::swap(nextMorning, nextEvening);
    }

    m_nextMorning->setStartDateTime(nextMorning->startDateTime());
    m_nextMorning->setEndDateTime(nextMorning->endDateTime());

    m_nextEvening->setStartDateTime(nextEvening->startDateTime());
    m_nextEvening->setEndDateTime(nextEvening->endDateTime());
}

DayNightTransition *DayNightSchedule::nextMorning() const
{
    return m_nextMorning.get();
}

DayNightTransition *DayNightSchedule::nextEvening() const
{
    return m_nextEvening.get();
}

#include "moc_daynightschedule.cpp"
