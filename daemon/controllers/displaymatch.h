/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QByteArray>

#include <optional>

/**
 * Stores data that can be used to match a single display well enough (alas, not perfectly)
 * across reboots and reconnects.
 */
struct DisplayMatch {
    int maxBrightness = 0;
    bool isInternal = false;
    std::optional<QByteArray> edidData;

    bool isValid() const
    {
        return maxBrightness > 0 && (isInternal || edidData.has_value());
    }

    // Unfortunately, `auto operator<=>(const DisplayMatch &other) const = default;` doesn't build
    // on KDE Invent's FreeBSD CI. Define comparison operators manually instead.
    bool operator==(const DisplayMatch &other) const = default;
    bool operator!=(const DisplayMatch &other) const = default;
    bool operator<(const DisplayMatch &other) const
    {
        return std::tie(maxBrightness, isInternal, edidData) < std::tie(other.maxBrightness, other.isInternal, other.edidData);
    }
    bool operator>(const DisplayMatch &other) const = default;
    bool operator<=(const DisplayMatch &other) const = default;
    bool operator>=(const DisplayMatch &other) const = default;
};

/**
 * Stores data that can be used to match one or more displays as part of a rule-driven behavior.
 */
struct DisplayFilter {
    std::optional<bool> isInternal;
    QList<QByteArray> includedEdids;
    QList<QByteArray> excludedEdids;
    bool includedByDefault = true;

    bool includes(const DisplayMatch &match) const
    {
        if (isInternal.has_value() && *isInternal != match.isInternal) {
            return false;
        }
        for (const QByteArray &includedEdid : std::as_const(includedEdids)) {
            if (includedEdid.isEmpty() || !match.edidData.has_value() || match.edidData->isEmpty()) {
                continue;
            }
            const bool isPrefixMatch = includedEdid.size() < match.edidData->size() // e.g. first 128 bytes vs. full EDID
                ? match.edidData->startsWith(includedEdid)
                : includedEdid.startsWith(*match.edidData);
            if (isPrefixMatch) {
                return true;
            }
        }
        for (const QByteArray &excludedEdid : std::as_const(excludedEdids)) {
            if (excludedEdid.isEmpty() || !match.edidData.has_value() || match.edidData->isEmpty()) {
                continue;
            }
            const bool isPrefixMatch = excludedEdid.size() < match.edidData->size() // e.g. first 128 bytes vs. full EDID
                ? match.edidData->startsWith(excludedEdid)
                : excludedEdid.startsWith(*match.edidData);
            if (isPrefixMatch) {
                return false;
            }
        }
        return includedByDefault;
    }

    // fluent API for quick & sufficiently verbose initialization
    DisplayFilter &includeByDefault(bool b)
    {
        includedByDefault = b;
        return *this;
    }
    DisplayFilter &isInternalEquals(bool b)
    {
        isInternal = b;
        return *this;
    }
    DisplayFilter &includeEdids(QList<QByteArray> edids)
    {
        includedEdids = std::move(edids);
        return *this;
    }
    DisplayFilter &excludeEdids(QList<QByteArray> edids)
    {
        excludedEdids = std::move(edids);
        return *this;
    }
};
