/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#pragma once

#include <powerdevilenums.h>

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace PowerDevil
{
class PowerManagement;
}

class SleepModeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        Name = Qt::DisplayRole,
        Subtitle = Qt::StatusTipRole,
        Value = Qt::UserRole
    };

    explicit SleepModeModel(QObject *parent, PowerDevil::PowerManagement *pm, bool isPowerSupplyBatteryPresent);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Data {
        QString name;
        QString subtitle;
        uint value;
    };

    QVector<Data> m_data;
};
