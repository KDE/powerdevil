/*
 * SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "screenbrightnessdisplaymodel.h"

#include <QAbstractListModel>

class ScreenBrightnessDisplayModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        LabelRole = Qt::DisplayRole,
        DisplayNameRole = Qt::UserRole,
        IsInternalRole,
        BrightnessRole,
        MaxBrightnessRole,
    };

    explicit ScreenBrightnessDisplayModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex displayIndex(const QString &displayName) const;
    void insertDisplay(const QString &displayName, const QModelIndex &index, const QString &label, bool isInternal, int brightness, int maxBrightness);
    void removeMissingDisplays(const QStringList &displayNames);

    void onBrightnessChanged(const QString &displayName, int value);
    void onBrightnessRangeChanged(const QString &displayName, int max, int value);

private:
    struct Data {
        QString displayName;
        QString label;
        int brightness;
        int maxBrightness;
        bool isInternal;
    };
    QStringList m_displayNames;
    QList<Data> m_displays;
};
