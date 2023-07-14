/*
    SPDX-FileCopyrightText: 2014 Martin Klapetek <mklapetek@kde.org>
    SPDX-FileCopyrightText: 2016 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QString>

#include <memory>

#include "osdaction.h"

class QQuickView;

class QTimer;

namespace PowerDevil
{
class Osd : public QObject
{
    Q_OBJECT

public:
    explicit Osd(QObject *parent = nullptr);
    ~Osd() override;

    void showActionSelector(QString currentProfile = QString());
    void hideOsd();

Q_SIGNALS:
    void osdActionSelected(QString action);

private Q_SLOTS:
    void onOsdActionSelected(QString action);

private:
    QQmlEngine m_engine;
    std::unique_ptr<QQuickView> m_osdActionSelector;
    QTimer *m_osdTimer = nullptr;
    int m_timeout = 0;
};

} // ns
