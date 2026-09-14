/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KDarkLightScheduleProvider>
#include <KSystemClockSkewNotifier>

#include <QTimer>
#include <qqmlregistration.h>

class DarkLightTransitionProgress : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QDateTime startDateTime READ startDateTime WRITE setStartDateTime NOTIFY startDateTimeChanged)
    Q_PROPERTY(QDateTime endDateTime READ endDateTime WRITE setEndDateTime NOTIFY endDateTimeChanged)

public:
    explicit DarkLightTransitionProgress(QObject *parent = nullptr);

    bool isActive() const;
    void setActive(bool active);

    QDateTime startDateTime() const;
    void setStartDateTime(const QDateTime &dateTime);

    QDateTime endDateTime() const;
    void setEndDateTime(const QDateTime &dateTime);

Q_SIGNALS:
    void activeChanged();
    void startDateTimeChanged();
    void endDateTimeChanged();

private:
    void reset();

    QTimer m_timer;
    KSystemClockSkewNotifier m_skewNotifier;

    QDateTime m_startDateTime;
    QDateTime m_endDateTime;
    bool m_active = false;
};
