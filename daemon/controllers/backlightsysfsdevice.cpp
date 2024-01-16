/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "backlightsysfsdevice.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

using namespace Qt::StringLiterals;

inline constexpr QLatin1StringView SYSFS_PATH("/sys/class/");
inline constexpr QLatin1StringView SYSFS_BACKLIGHT_PATH("/sys/class/backlight/");
inline constexpr QLatin1StringView SYSFS_BACKLIGHT_SUBSYSTEM("backlight");
inline constexpr QLatin1StringView SYSFS_LEDS_PATH("/sys/class/leds/");
inline constexpr QLatin1StringView SYSFS_LEDS_SUBSYSTEM("leds");

// static
QString BacklightSysfsDevice::syspath(const QString &subsystem, const QString &interface)
{
    return SYSFS_PATH + subsystem + u'/' + interface;
}

// static
QList<BacklightSysfsDevice> BacklightSysfsDevice::getBacklightTypeDevices()
{
    auto appendIfMaxBrightnessAvailable = [](QList<BacklightSysfsDevice> &list, QString subsystem, QString interface) {
        QString syspath = BacklightSysfsDevice::syspath(subsystem, interface);
        std::optional<int> maxBrightness = BacklightSysfsDevice::readInt(syspath, "max_brightness"_L1);
        if (maxBrightness.has_value() && *maxBrightness > 0) {
            list.append(BacklightSysfsDevice{
                .subsystem = std::move(subsystem),
                .interface = std::move(interface),
                .maxBrightness = *maxBrightness,
            });
        }
    };

    QDir ledsDir(SYSFS_LEDS_PATH);
    ledsDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    ledsDir.setNameFilters({QStringLiteral("*lcd*"), QStringLiteral("*wled*")});

    QStringList ledInterfaces = ledsDir.entryList();

    if (!ledInterfaces.isEmpty()) {
        QList<BacklightSysfsDevice> output;
        for (const QString &interface : ledInterfaces) {
            appendIfMaxBrightnessAvailable(output, SYSFS_LEDS_SUBSYSTEM, interface);
        }
        return output;
    }

    QDir backlightDir(SYSFS_BACKLIGHT_PATH);
    backlightDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable);
    backlightDir.setSorting(QDir::Name | QDir::Reversed); // Reverse is needed to prioritize acpi_video1 over 0

    const QStringList interfaces = backlightDir.entryList();

    QFile file;
    QByteArray buffer;
    QList<BacklightSysfsDevice> firmware, platform, rawEnabled, rawAll;

    for (const QString &interface : interfaces) {
        file.setFileName(SYSFS_BACKLIGHT_PATH + interface + u"/type");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        buffer = file.readLine().trimmed();
        if (buffer == "firmware") {
            appendIfMaxBrightnessAvailable(firmware, SYSFS_BACKLIGHT_SUBSYSTEM, interface);
        } else if (buffer == "platform") {
            appendIfMaxBrightnessAvailable(platform, SYSFS_BACKLIGHT_SUBSYSTEM, interface);
        } else if (buffer == "raw") {
            QFile enabled(SYSFS_BACKLIGHT_PATH + interface + u"/device/enabled");
            appendIfMaxBrightnessAvailable(rawAll, SYSFS_BACKLIGHT_SUBSYSTEM, interface);
            if (enabled.open(QIODevice::ReadOnly | QIODevice::Text) && enabled.readLine().trimmed() == "enabled") {
                // this backlight device is connected to a display, so append
                // it to rawEnabled list
                appendIfMaxBrightnessAvailable(rawEnabled, SYSFS_BACKLIGHT_SUBSYSTEM, interface);
            }
        } else {
            qCWarning(POWERDEVIL) << "Interface type not handled" << buffer;
        }

        file.close();
    }

    if (!firmware.isEmpty()) {
        return firmware;
    }
    if (!platform.isEmpty()) {
        return platform;
    }
    if (!rawEnabled.isEmpty()) {
        return rawEnabled;
    }
    if (!rawAll.isEmpty()) {
        return rawAll;
    }
    return {};
}

// static
std::optional<int> BacklightSysfsDevice::readInt(const QString &syspath, const QLatin1StringView property)
{
    QFile file(QString(syspath + u'/' + property));
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(POWERDEVIL).nospace().noquote() << "reading from device " << syspath << "/" << property << " failed with error code " << file.error() << ":"
                                                  << file.errorString();
        return std::nullopt;
    }

    int value = -1;

    QTextStream stream(&file);
    stream >> value;
    file.close();

    return stream.status() == QTextStream::Ok ? std::make_optional(value) : std::nullopt;
}

QString BacklightSysfsDevice::syspath() const
{
    return BacklightSysfsDevice::syspath(subsystem, interface);
}

std::optional<int> BacklightSysfsDevice::readInt(const QLatin1StringView property) const
{
    return BacklightSysfsDevice::readInt(syspath(), property);
}

std::optional<int> BacklightSysfsDevice::readBrightness() const
{
    std::optional<int> br = readInt("brightness"_L1);
    return (br.has_value() && *br >= 0) ? br : std::nullopt;
}

#include "moc_backlightsysfsdevice.cpp"
