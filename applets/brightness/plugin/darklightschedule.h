/*
 * SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KDarkLightScheduleProvider>

#include <QJSValue>
#include <qqmlregistration.h>

class DarkLightSchedule
{
    Q_GADGET
    QML_VALUE_TYPE(darkLightSchedule)

public:
    DarkLightSchedule();
    explicit DarkLightSchedule(const KDarkLightSchedule &schedule);

    Q_INVOKABLE QDateTime nextTransition(const QDateTime &referenceDateTime) const;

private:
    KDarkLightSchedule m_schedule;
};

class DarkLightScheduleProvider : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit DarkLightScheduleProvider(QObject *parent = nullptr);

    Q_INVOKABLE void poll(QJSValue callback);
};
