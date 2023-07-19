/*  This file is part of the KDE project
 *    Copyright (C) 2010-2011 Lukas Tinkl <ltinkl@redhat.com>
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License version 2 as published by the Free Software Foundation.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 *
 */

#include "backlighthelper.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QDir>

#include <KLocalizedString>

#include <algorithm>
#include <climits>
#include <sys/utsname.h>

#ifdef Q_OS_FREEBSD
#define USE_SYSCTL
#endif

#ifdef USE_SYSCTL
#include <sys/sysctl.h>
#include <sys/types.h>

#define HAS_SYSCTL(n) (sysctlbyname(n, nullptr, nullptr, nullptr, 0) == 0)
#endif

#define BACKLIGHT_SYSFS_PATH "/sys/class/backlight/"
#define LED_SYSFS_PATH "/sys/class/leds/"

BacklightHelper::BacklightHelper(QObject *parent)
    : QObject(parent)
{
    init();
}

void BacklightHelper::init()
{
    initUsingBacklightType();

    if (m_devices.isEmpty()) {
        initUsingSysctl();

        if (m_sysctlDevice.isEmpty() || m_sysctlBrightnessLevels.isEmpty()) {
            qCWarning(POWERDEVIL) << "no kernel backlight interface found";
            return;
        }
    }

    m_anim.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        // When animating to zero, it emits a value change to 0 before starting the animation...
        if (m_anim.state() == QAbstractAnimation::Running) {
            writeBrightness(value.toInt());
        }
    });

    m_isSupported = true;
}

int BacklightHelper::readFromDevice(const QString &device, const QString &property) const
{
    int value;
    QFile file(device + "/" + property);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(POWERDEVIL) << "reading from device " << device << "/" << property << " failed with error code " << file.error() << file.errorString();
        return -1;
    }

    QTextStream stream(&file);
    stream >> value;
    file.close();
    return value;
}

bool BacklightHelper::writeToDevice(const QString &device, int brightness) const
{
    QFile file(device + QLatin1String("/brightness"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(POWERDEVIL) << "writing to device " << device << "/brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    const int bytesWritten = file.write(QByteArray::number(brightness));
    if (bytesWritten == -1) {
        qCWarning(POWERDEVIL) << "writing to device " << device << "/brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    return true;
}

QStringList BacklightHelper::getBacklightTypeDevices() const
{
    QDir ledsDir(LED_SYSFS_PATH);
    ledsDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
    ledsDir.setNameFilters({QStringLiteral("*lcd*"), QStringLiteral("*wled*")});

    QStringList ledInterfaces = ledsDir.entryList();

    if (!ledInterfaces.isEmpty()) {
        QStringList output;
        for (const QString &interface : ledInterfaces) {
            output.append(LED_SYSFS_PATH + interface);
        }
        return output;
    }

    QDir backlightDir(BACKLIGHT_SYSFS_PATH);
    backlightDir.setFilter(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
    backlightDir.setSorting(QDir::Name | QDir::Reversed); // Reverse is needed to priorize acpi_video1 over 0

    const QStringList interfaces = backlightDir.entryList();

    QFile file;
    QByteArray buffer;
    QStringList firmware, platform, rawEnabled, rawAll;

    for (const QString &interface : interfaces) {
        file.setFileName(BACKLIGHT_SYSFS_PATH + interface + "/type");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        buffer = file.readLine().trimmed();
        if (buffer == "firmware") {
            firmware.append(BACKLIGHT_SYSFS_PATH + interface);
        } else if (buffer == "platform") {
            platform.append(BACKLIGHT_SYSFS_PATH + interface);
        } else if (buffer == "raw") {
            QFile enabled(BACKLIGHT_SYSFS_PATH + interface + "/device/enabled");
            rawAll.append(BACKLIGHT_SYSFS_PATH + interface);
            if (enabled.open(QIODevice::ReadOnly | QIODevice::Text) && enabled.readLine().trimmed() == "enabled") {
                // this backlight device is connected to a display, so append
                // it to rawEnabled list
                rawEnabled.append(BACKLIGHT_SYSFS_PATH + interface);
            }
        } else {
            qCWarning(POWERDEVIL) << "Interface type not handled" << buffer;
        }

        file.close();
    }

    if (!firmware.isEmpty())
        return firmware;

    if (!platform.isEmpty())
        return platform;

    if (!rawEnabled.isEmpty())
        return rawEnabled;

    if (!rawAll.isEmpty())
        return rawAll;

    return {};
}

void BacklightHelper::initUsingBacklightType()
{
    m_devices.clear();
    QStringList devices = getBacklightTypeDevices();

    for (const QString &interface : devices) {
        int max_brightness = readFromDevice(interface, "max_brightness");
        m_devices.append(qMakePair(interface, max_brightness));
    }

    return;
}

void BacklightHelper::initUsingSysctl()
{
#ifdef USE_SYSCTL
    /*
     * lcd0 is, in theory, the correct device, but some vendors have custom ACPI implementations
     * which cannot be interpreted. In that case, devices should be reported as "out", but
     * FreeBSD doesn't care (yet), so they can appear as any other type. Let's search for the first
     * device with brightness management, then.
     */
    QStringList types;
    types << QStringLiteral("lcd") << QStringLiteral("out") << QStringLiteral("crt") << QStringLiteral("tv") << QStringLiteral("ext");
    for (const QString &type : types) {
        for (int i = 0; m_sysctlDevice.isEmpty(); i++) {
            QString device = QStringLiteral("%1%2").arg(type, QString::number(i));
            // We don't care about the value, we only want the sysctl to be there.
            if (!HAS_SYSCTL(qPrintable(QStringLiteral("hw.acpi.video.%1.active").arg(device)))) {
                break;
            }
            if (HAS_SYSCTL(qPrintable(QStringLiteral("hw.acpi.video.%1.brightness").arg(device)))
                && HAS_SYSCTL(qPrintable(QStringLiteral("hw.acpi.video.%1.levels").arg(device)))) {
                m_sysctlDevice = device;
                break;
            }
        }
    }

    if (m_sysctlDevice.isEmpty()) {
        return;
    }

    size_t len;
    if (sysctlbyname(qPrintable(QStringLiteral("hw.acpi.video.%1.levels").arg(m_sysctlDevice)), nullptr, &len, nullptr, 0) != 0 || len == 0) {
        return;
    }
    int *levels = (int *)malloc(len);
    if (!levels) {
        return;
    }
    if (sysctlbyname(qPrintable(QString("hw.acpi.video.%1.levels").arg(m_sysctlDevice)), levels, &len, nullptr, 0) != 0) {
        free(levels);
        return;
    }
    // acpi_video(4) supports only some predefined brightness levels.
    int nlevels = len / sizeof(int);
    for (int i = 0; i < nlevels; i++) {
        m_sysctlBrightnessLevels << levels[i];
    }
    free(levels);
    // Sorting helps when finding max value and when scanning for the nearest level in setbrightness().
    std::sort(m_sysctlBrightnessLevels.begin(), m_sysctlBrightnessLevels.end());
#endif
}

ActionReply BacklightHelper::brightness(const QVariantMap &args)
{
    Q_UNUSED(args);
    const int brightness = readBrightness();

    if (brightness == -1) {
        return ActionReply::HelperErrorReply();
    }

    ActionReply reply;
    reply.addData(QStringLiteral("brightness"), brightness);
    return reply;
}

int BacklightHelper::readBrightness() const
{
    if (!m_isSupported) {
        return -1;
    }

    int brightness;

#ifdef USE_SYSCTL
    size_t len = sizeof(int);
    if (sysctlbyname(qPrintable(QStringLiteral("hw.acpi.video.%1.brightness").arg(m_sysctlDevice)), &brightness, &len, nullptr, 0) != 0) {
        return -1;
    }
#else
    brightness = readFromDevice(m_devices.constFirst().first, QLatin1String("brightness"));
#endif

    return brightness;
}

ActionReply BacklightHelper::setbrightness(const QVariantMap &args)
{
    if (!m_isSupported) {
        return ActionReply::HelperErrorReply();
    }

    const int brightness = args.value(QStringLiteral("brightness")).toInt();
    const int animationDuration = args.value(QStringLiteral("animationDuration")).toInt();

    m_anim.stop();

    if (animationDuration <= 0) {
        writeBrightness(brightness);
        return ActionReply::SuccessReply();
    }

    m_anim.setDuration(animationDuration);
    m_anim.setStartValue(readBrightness());
    m_anim.setEndValue(brightness);
    m_anim.start();

    return ActionReply::SuccessReply();
}

bool BacklightHelper::writeBrightness(int brightness) const
{
#ifdef USE_SYSCTL
    int actual_level = -1;
    int d1 = 101;
    // Search for the nearest level.
    for (int level : m_sysctlBrightnessLevels) {
        int d2 = qAbs(level - brightness);
        /*
         * The list is sorted, so we break when it starts diverging. There may be repeated values,
         * so we keep going on equal gap (e.g., value = 7.5, levels = 0 0 10 ...: we don't break at
         * the second '0' so we can get to the '10'). This also means that the value will always
         * round off to the bigger level when in the middle (e.g., value = 5, levels = 0 10 ...:
         * value rounds off to 10).
         */
        if (d2 > d1) {
            break;
        }
        actual_level = level;
        d1 = d2;
    }
    size_t len = sizeof(int);
    return sysctlbyname(qPrintable(QStringLiteral("hw.acpi.video.%1.brightness").arg(m_sysctlDevice)), nullptr, nullptr, &actual_level, len) == 0;
#else

    if (!m_devices.isEmpty()) {
        const int first_maxbrightness = std::max(1, m_devices.constFirst().second);
        for (const auto &device : m_devices) {
            // Some monitor brightness values are ridiculously high, and can easily overflow during computation
            const qint64 new_brightness_64 = static_cast<qint64>(brightness) * static_cast<qint64>(device.second) / static_cast<qint64>(first_maxbrightness);
            // cautiously truncate it back
            const int new_brightness = static_cast<int>(std::min(static_cast<qint64>(std::numeric_limits<int>::max()), new_brightness_64));
            writeToDevice(device.first, new_brightness);
        }
    }

    return true;
#endif

    return false;
}

ActionReply BacklightHelper::syspath(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported || m_devices.isEmpty()) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData(QStringLiteral("syspath"), m_devices.constFirst().first);

    return reply;
}

ActionReply BacklightHelper::brightnessmax(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return -1;
    }

    // maximum brightness
    int max_brightness;

#ifdef USE_SYSCTL
    max_brightness = m_sysctlBrightnessLevels.last();
#else
    max_brightness = readFromDevice(m_devices.constFirst().first, QLatin1String("max_brightness"));
#endif

    if (max_brightness <= 0) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData(QStringLiteral("brightnessmax"), max_brightness);
    // qCDebug(POWERDEVIL) << "data contains:" << reply.data()["brightnessmax"];

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.backlighthelper", BacklightHelper)

#include "moc_backlighthelper.cpp"
