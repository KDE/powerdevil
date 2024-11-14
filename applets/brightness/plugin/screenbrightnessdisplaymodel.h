/*
 * SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "screenbrightnessdisplaymodel.h"

#include <QAbstractListModel>
#include <QMap>

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

    // Shown display names (i.e. included in the model, with valid index) are displays included in
    // known display names that also have display data set. Data for any display outside the list of
    // known display names will be removed / not set.
    void setKnownDisplayNames(const QStringList &displayNames);
    void setDisplayData(const QString &displayName, const QString &label, bool isInternal, int brightness, int maxBrightness);
    QStringList knownDisplayNamesWithMissingData() const;

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

    void updateRows();

    QStringList m_knownDisplayNames;
    QStringList m_shownDisplayNames;
    QMap<QString, Data> m_displays;
};
