/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "darklighttransitionprogress.h"

using namespace std::chrono_literals;

DarkLightTransitionProgress::DarkLightTransitionProgress(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &DarkLightTransitionProgress::reset);
    m_timer.setSingleShot(true);

    connect(&m_skewNotifier, &KSystemClockSkewNotifier::skewed, this, &DarkLightTransitionProgress::reset);
}

bool DarkLightTransitionProgress::isActive() const
{
    return m_active;
}

void DarkLightTransitionProgress::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        Q_EMIT activeChanged();
    }
}

QDateTime DarkLightTransitionProgress::startDateTime() const
{
    return m_startDateTime;
}

void DarkLightTransitionProgress::setStartDateTime(const QDateTime &dateTime)
{
    if (m_startDateTime != dateTime) {
        m_startDateTime = dateTime;
        reset();
        Q_EMIT startDateTimeChanged();
    }
}

QDateTime DarkLightTransitionProgress::endDateTime() const
{
    return m_endDateTime;
}

void DarkLightTransitionProgress::setEndDateTime(const QDateTime &dateTime)
{
    if (m_endDateTime != dateTime) {
        m_endDateTime = dateTime;
        reset();
        Q_EMIT endDateTimeChanged();
    }
}

void DarkLightTransitionProgress::reset()
{
    if (m_startDateTime.isNull() || m_endDateTime.isNull()) {
        m_timer.stop();
        m_skewNotifier.setActive(false);
    } else {
        if (const auto duration = m_startDateTime - QDateTime::currentDateTime(); duration >= 1min) {
            m_timer.start(duration);
            m_skewNotifier.setActive(true);
        } else if (const auto duration = m_endDateTime - QDateTime::currentDateTime(); duration >= 1min) {
            m_timer.start(duration);
            m_skewNotifier.setActive(true);
        } else {
            m_timer.stop();
            m_skewNotifier.setActive(false);
        }
    }

    if (m_startDateTime.isNull() || m_endDateTime.isNull()) {
        setActive(false);
    } else {
        const QDateTime now = QDateTime::currentDateTime();
        const bool passedStartDateTime = m_startDateTime - now < 1min;
        const bool passedEndDateTime = m_endDateTime - now < 1min;
        setActive(passedStartDateTime && !passedEndDateTime);
    }
}

#include "moc_darklighttransitionprogress.cpp"
