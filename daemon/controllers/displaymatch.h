/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QByteArray>

#include <optional>

/**
 * Stores data that can be used to match displays well enough (alas, not perfectly) across
 * reboots and reconnects.
 */
struct DisplayMatch {
    int maxBrightness = 0;
    bool isInternal = false;
    std::optional<QByteArray> edidData;

    bool isValid() const
    {
        return maxBrightness > 0 && (isInternal || edidData.has_value());
    }

    bool matchesExcludingMaxBrightness(const DisplayMatch &other) const
    {
        if (isInternal && other.isInternal) {
            return true;
        }
        if (edidData.has_value() && other.edidData.has_value()) {
            // EDID data could be the mandatory first 128 bytes, or the complete EDID with extension chunks.
            return edidData->size() > other.edidData->size() ? edidData->startsWith(*other.edidData) : other.edidData->startsWith(*edidData);
        }
        return false;
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
