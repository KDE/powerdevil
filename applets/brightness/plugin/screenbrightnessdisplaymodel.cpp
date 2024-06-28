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

    return m_displayIds.count();
}

QVariant ScreenBrightnessDisplayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    switch (role) {
    case DisplayIdRole:
        return m_displayIds[index.row()];
    case LabelRole:
        return m_displays[index.row()].label;
    case BrightnessRole:
        return m_displays[index.row()].brightness;
    case MinBrightnessRole:
        return m_displays[index.row()].minBrightness;
    case MaxBrightnessRole:
        return m_displays[index.row()].maxBrightness;
    }
    return QVariant();
}

QHash<int, QByteArray> ScreenBrightnessDisplayModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {DisplayIdRole, "displayId"},
        {LabelRole, "label"},
        {BrightnessRole, "brightness"},
        {MinBrightnessRole, "minBrightness"},
        {MaxBrightnessRole, "maxBrightness"},
    };
}

QModelIndex ScreenBrightnessDisplayModel::displayIndex(const QString &displayId) const
{
    int index = m_displayIds.indexOf(displayId);
    return index == -1 ? QModelIndex() : QAbstractListModel::createIndex(index, 0);
}

void ScreenBrightnessDisplayModel::appendDisplay(const QString &displayId, const QString &label, int brightness, int minBrightness, int maxBrightness)
{
    beginInsertRows(QModelIndex(), m_displayIds.count(), m_displayIds.count());
    m_displayIds.append(displayId);
    m_displays.append({
        .displayId = displayId,
        .label = label,
        .brightness = brightness,
        .minBrightness = minBrightness,
        .maxBrightness = maxBrightness,
    });
    endInsertRows();
}

void ScreenBrightnessDisplayModel::removeDisplay(const QString &displayId)
{
    auto index = m_displayIds.indexOf(displayId);
    if (index != -1) {
        beginRemoveRows(QModelIndex(), index, index);
        m_displayIds.remove(index);
        m_displays.remove(index);
        endRemoveRows();
    }
}

void ScreenBrightnessDisplayModel::onBrightnessChanged(const QString &displayId, int value)
{
    QModelIndex modelIndex = displayIndex(displayId);
    if (modelIndex.isValid()) {
        m_displays[modelIndex.row()].brightness = value;
        dataChanged(modelIndex, modelIndex, {BrightnessRole});
    }
}

void ScreenBrightnessDisplayModel::onBrightnessRangeChanged(const QString &displayId, int min, int max, int value)
{
    QModelIndex modelIndex = displayIndex(displayId);
    if (modelIndex.isValid()) {
        auto &display = m_displays[modelIndex.row()];
        display.minBrightness = min;
        display.maxBrightness = max;
        display.brightness = value;
        dataChanged(modelIndex, modelIndex, {BrightnessRole, MinBrightnessRole, MaxBrightnessRole});
    }
}

#include "moc_screenbrightnessdisplaymodel.cpp"
