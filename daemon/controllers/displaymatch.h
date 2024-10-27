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

    auto operator<=>(const DisplayMatch &) const = default;

    bool isValid() const
    {
        return maxBrightness > 0 && (isInternal || edidData.has_value());
    }
};
