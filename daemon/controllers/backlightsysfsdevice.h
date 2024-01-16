/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2010-2011 Lukas Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include <QList>
#include <QPair>
#include <QString>

#include <optional>

/**
 * Linux-specific helper struct to access sysfs devices with backlight brightness functionality.
 */
struct BacklightSysfsDevice {
    QString subsystem;
    QString interface;
    int maxBrightness;

    QString syspath() const;

    // accessors return std::nullopt if unavailable
    std::optional<int> readInt(const QLatin1StringView property) const;
    std::optional<int> readBrightness() const;

    static QString syspath(const QString &subsystem, const QString &interface);

    /**
     * BacklightBrightness will be backed by one or more sysfs backlight devices.
     * The kernel starting from version 2.6.37 offers the type of the interface, and based on that
     * we can decide which interface is better for us, being the order firmware-platform-raw.
     */
    static QList<BacklightSysfsDevice> getBacklightTypeDevices();

    static std::optional<int> readInt(const QString &syspath, const QLatin1StringView property);
};
