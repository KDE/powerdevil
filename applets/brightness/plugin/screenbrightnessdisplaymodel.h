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
        DisplayIdRole = Qt::UserRole,
        IsInternalRole,
        BrightnessRole,
        MaxBrightnessRole,
    };

    explicit ScreenBrightnessDisplayModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex displayIndex(const QString &displayId) const;
    void appendDisplay(const QString &displayId, const QString &label, bool isInternal, int brightness, int maxBrightness);
    void removeDisplay(const QString &displayId);

    void onBrightnessChanged(const QString &displayId, int value);
    void onBrightnessRangeChanged(const QString &displayId, int max, int value);

private:
    struct Data {
        QString displayId;
        QString label;
        int brightness;
        int maxBrightness;
        bool isInternal;
    };
    QStringList m_displayIds;
    QList<Data> m_displays;
};
