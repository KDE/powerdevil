/*
 * SPDX-FileCopyrightText: 2025 Bhushan Shah <bshah@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>

#include <KAuth/ActionReply>

using namespace KAuth;

class WakeupSourceHelper : public QObject
{
    Q_OBJECT
public:
    explicit WakeupSourceHelper(QObject *parent = nullptr);

public Q_SLOTS:
    ActionReply setwakeupcount(const QVariantMap &args);
};
