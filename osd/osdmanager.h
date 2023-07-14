/*
    SPDX-FileCopyrightText: 2016 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusContext>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>

#include "osdaction.h"

namespace PowerDevil
{
class ConfigOperation;
class Osd;

class OsdManager : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    OsdManager(QObject *parent = nullptr);
    ~OsdManager() override;

public Q_SLOTS:
    void hideOsd();
    void showOsd();
    void applyProfile(QString profile);

private:
    void quit();
    QTimer *m_cleanupTimer;
};

} // ns
