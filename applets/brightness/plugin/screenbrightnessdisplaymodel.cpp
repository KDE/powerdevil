/*
 * SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnessdisplaymodel.h"

ScreenBrightnessDisplayModel::ScreenBrightnessDisplayModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScreenBrightnessDisplayModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_displayNames.count();
}

QVariant ScreenBrightnessDisplayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    switch (role) {
    case DisplayNameRole:
        return m_displayNames[index.row()];
    case LabelRole:
        return m_displays[index.row()].label;
    case IsInternalRole:
        return m_displays[index.row()].isInternal;
    case BrightnessRole:
        return m_displays[index.row()].brightness;
    case MaxBrightnessRole:
        return m_displays[index.row()].maxBrightness;
    }
    return QVariant();
}

QHash<int, QByteArray> ScreenBrightnessDisplayModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {DisplayNameRole, "displayName"},
        {LabelRole, "label"},
        {IsInternalRole, "isInternal"},
        {BrightnessRole, "brightness"},
        {MaxBrightnessRole, "maxBrightness"},
    };
}

QModelIndex ScreenBrightnessDisplayModel::displayIndex(const QString &displayName) const
{
    int row = m_displayNames.indexOf(displayName);
    return row == -1 ? QModelIndex() : QAbstractListModel::createIndex(row, 0);
}

void ScreenBrightnessDisplayModel::insertDisplay(const QString &displayName,
                                                 const QModelIndex &index,
                                                 const QString &label,
                                                 bool isInternal,
                                                 int brightness,
                                                 int maxBrightness)
{
    int row = index.isValid() ? index.row() : m_displayNames.size();

    beginInsertRows(QModelIndex(), row, row);
    m_displayNames.insert(row, displayName);
    m_displays.insert(row,
                      {
                          .displayName = displayName,
                          .label = label,
                          .brightness = brightness,
                          .maxBrightness = maxBrightness,
                          .isInternal = isInternal,
                      });
    endInsertRows();
}

void ScreenBrightnessDisplayModel::removeMissingDisplays(const QStringList &displayNames)
{
    for (int row = 0; row < m_displayNames.size(); ++row) {
        if (!displayNames.contains(m_displayNames.at(row))) {
            beginRemoveRows(QModelIndex(), row, row);
            m_displayNames.remove(row);
            m_displays.remove(row);
            endRemoveRows();
            --row;
        }
    }
}

void ScreenBrightnessDisplayModel::onBrightnessChanged(const QString &displayName, int value)
{
    QModelIndex modelIndex = displayIndex(displayName);
    if (modelIndex.isValid()) {
        m_displays[modelIndex.row()].brightness = value;
        dataChanged(modelIndex, modelIndex, {BrightnessRole});
    }
}

void ScreenBrightnessDisplayModel::onBrightnessRangeChanged(const QString &displayName, int max, int value)
{
    QModelIndex modelIndex = displayIndex(displayName);
    if (modelIndex.isValid()) {
        auto &display = m_displays[modelIndex.row()];
        display.maxBrightness = max;
        display.brightness = value;
        dataChanged(modelIndex, modelIndex, {MaxBrightnessRole, BrightnessRole});
    }
}

#include "moc_screenbrightnessdisplaymodel.cpp"
