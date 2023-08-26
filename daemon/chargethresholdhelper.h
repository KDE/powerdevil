/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>

#include <KAuth/ActionReply>

using namespace KAuth;

class ChargeThresholdHelper : public QObject
{
    Q_OBJECT
public:
    explicit ChargeThresholdHelper(QObject *parent = nullptr);

public Q_SLOTS:
    ActionReply getthreshold(const QVariantMap &args);
    ActionReply setthreshold(const QVariantMap &args);
};
