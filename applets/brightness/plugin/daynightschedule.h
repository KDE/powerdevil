/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KDarkLightScheduleProvider>

#include <QDateTime>
#include <QObject>
#include <qqmlregistration.h>

class DayNightTransition : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QDateTime startDateTime READ startDateTime NOTIFY startDateTimeChanged)
    Q_PROPERTY(QDateTime endDateTime READ endDateTime NOTIFY endDateTimeChanged)

public:
    explicit DayNightTransition(QObject *parent = nullptr);

    QDateTime startDateTime() const;
    void setStartDateTime(const QDateTime &dateTime);

    QDateTime endDateTime() const;
    void setEndDateTime(const QDateTime &dateTime);

Q_SIGNALS:
    void startDateTimeChanged();
    void endDateTimeChanged();

private:
    QDateTime m_startDateTime;
    QDateTime m_endDateTime;
};

class DayNightSchedule : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(DayNightTransition *nextMorning READ nextMorning CONSTANT)
    Q_PROPERTY(DayNightTransition *nextEvening READ nextEvening CONSTANT)

public:
    explicit DayNightSchedule(QObject *parent = nullptr);
    ~DayNightSchedule() override;

    DayNightTransition *nextMorning() const;
    DayNightTransition *nextEvening() const;

private:
    void update();

    std::unique_ptr<KDarkLightScheduleProvider> m_provider;

    std::unique_ptr<DayNightTransition> m_nextMorning;
    std::unique_ptr<DayNightTransition> m_nextEvening;
};
