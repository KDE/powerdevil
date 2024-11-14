/*
 * SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnessdisplaymodel.h"

#include <ranges>

ScreenBrightnessDisplayModel::ScreenBrightnessDisplayModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ScreenBrightnessDisplayModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_shownDisplayNames.count();
}

QVariant ScreenBrightnessDisplayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shownDisplayNames.size()) {
        return QVariant();
    }
    const QString &displayName = m_shownDisplayNames[index.row()];

    switch (role) {
    case DisplayNameRole:
        return displayName;
    case LabelRole:
        return m_displays[displayName].label;
    case IsInternalRole:
        return m_displays[displayName].isInternal;
    case BrightnessRole:
        return m_displays[displayName].brightness;
    case MaxBrightnessRole:
        return m_displays[displayName].maxBrightness;
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
    int row = m_shownDisplayNames.indexOf(displayName);
    return row == -1 ? QModelIndex() : QAbstractListModel::createIndex(row, 0);
}

void ScreenBrightnessDisplayModel::setKnownDisplayNames(const QStringList &displayNames)
{
    m_knownDisplayNames = displayNames;
    updateRows();
}

void ScreenBrightnessDisplayModel::setDisplayData(const QString &displayName, const QString &label, bool isInternal, int brightness, int maxBrightness)
{
    if (!m_knownDisplayNames.contains(displayName)) {
        return;
    }

    m_displays[displayName] = Data{
        .displayName = displayName,
        .label = label,
        .brightness = brightness,
        .maxBrightness = maxBrightness,
        .isInternal = isInternal,
    };

    if (QModelIndex modelIndex = displayIndex(displayName); modelIndex.isValid()) {
        Q_EMIT dataChanged(modelIndex, modelIndex, {LabelRole, IsInternalRole, BrightnessRole, MaxBrightnessRole});
    } else {
        updateRows();
    }
}

void ScreenBrightnessDisplayModel::updateRows()
{
    QMap<QString, Data> filteredDisplays = m_displays;

    // Purge data for displays that were removed from the list of known display names.
    for (const QString &displayName : std::ranges::subrange(m_displays.keyBegin(), m_displays.keyEnd())) {
        if (!m_knownDisplayNames.contains(displayName)) {
            filteredDisplays.remove(displayName);
        }
    }

    // Iterate through m_knownDisplayNames and ensure that m_shownDisplayNames contains a subset
    // of displays (including displays whose data was set) in the same order.
    int row = 0;
    for (const QString &displayName : std::as_const(m_knownDisplayNames)) {
        const bool hasDisplayData = filteredDisplays.contains(displayName);

        if (row < m_shownDisplayNames.size() && m_shownDisplayNames[row] == displayName) {
            // This display name is already at the expected row index in m_shownDisplayNames.
            // Advance to the next row, or remove the row if its display data is missing.
            if (hasDisplayData) {
                ++row;
            } else {
                beginRemoveRows(QModelIndex(), row, row);
                m_shownDisplayNames.remove(row);
                endRemoveRows();
            }
            continue;
        }

        if (hasDisplayData) {
            // This display name needs to be inserted with the current (expected) row index.
            beginInsertRows(QModelIndex(), row, row);
            m_shownDisplayNames.insert(row, displayName);
            endInsertRows();
            ++row;
        }
    }

    // Any remaining elements in m_shownDisplayNames are not in m_knownDisplayNames, or were
    // not located in the correct row so they were pushed back by a newly inserted duplicate.
    if (row < m_shownDisplayNames.size()) {
        beginRemoveRows(QModelIndex(), row, m_shownDisplayNames.size() - 1);
        m_shownDisplayNames.resize(row);
        endRemoveRows();
    }

    m_displays = std::move(filteredDisplays);
}

QStringList ScreenBrightnessDisplayModel::knownDisplayNamesWithMissingData() const
{
    QStringList result;
    for (const QString &displayName : std::as_const(m_knownDisplayNames)) {
        if (!m_displays.contains(displayName)) {
            result.append(displayName);
        }
    }
    return result;
}

void ScreenBrightnessDisplayModel::onBrightnessChanged(const QString &displayName, int value)
{
    if (auto it = m_displays.find(displayName); it != m_displays.end()) {
        it->brightness = value;
        if (QModelIndex modelIndex = displayIndex(displayName); modelIndex.isValid()) {
            Q_EMIT dataChanged(modelIndex, modelIndex, {BrightnessRole});
        }
    }
}

void ScreenBrightnessDisplayModel::onBrightnessRangeChanged(const QString &displayName, int max, int value)
{
    if (auto it = m_displays.find(displayName); it != m_displays.end()) {
        it->maxBrightness = max;
        it->brightness = value;
        if (QModelIndex modelIndex = displayIndex(displayName); modelIndex.isValid()) {
            Q_EMIT dataChanged(modelIndex, modelIndex, {MaxBrightnessRole, BrightnessRole});
        }
    }
}

#include "moc_screenbrightnessdisplaymodel.cpp"
