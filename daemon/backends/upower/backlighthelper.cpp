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

#include <QDir>
#include <QDebug>

#include <KLocalizedString>

#include <algorithm>
#include <sys/utsname.h>

#ifdef Q_OS_FREEBSD
#define USE_SYSCTL
#endif

#ifdef USE_SYSCTL
#include <sys/types.h>
#include <sys/sysctl.h>

#define HAS_SYSCTL(n) (sysctlbyname(n, nullptr, nullptr, nullptr, 0) == 0)
#endif

#define BACKLIGHT_SYSFS_PATH "/sys/class/backlight/"
#define LED_SYSFS_PATH "/sys/class/leds/"

BacklightHelper::BacklightHelper(QObject *parent) : QObject(parent)
{
    init();
}

void BacklightHelper::init()
{
    initUsingBacklightType();

    if (m_dirname.isEmpty()) {
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

void BacklightHelper::initUsingBacklightType()
{
    QDir backlightDir(BACKLIGHT_SYSFS_PATH);
    backlightDir.setFilter(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
    backlightDir.setSorting(QDir::Name | QDir::Reversed);// Reverse is needed to priorize acpi_video1 over 0

    const QStringList interfaces = backlightDir.entryList();

    QFile file;
    QByteArray buffer;
    QStringList firmware, platform, raw, leds;

    for (const QString & interface : interfaces) {
        QFile enabled(BACKLIGHT_SYSFS_PATH + interface + "/device/enabled");
        if (!enabled.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        if (enabled.readLine().trimmed() != "enabled") {
            // this backlight device isn't connected to a display, so move on
            // to the next one and see if it does.
            continue;
        }

        file.setFileName(BACKLIGHT_SYSFS_PATH + interface + "/type");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        buffer = file.readLine().trimmed();
        if (buffer == "firmware") {
            firmware.append(interface);
        } else if(buffer == "platform") {
            platform.append(interface);
        } else if (buffer == "raw") {
            raw.append(interface);
        } else {
            qCWarning(POWERDEVIL) << "Interface type not handled" << buffer;
        }

        file.close();
    }

    QDir ledsDir(LED_SYSFS_PATH);
    ledsDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
    ledsDir.setNameFilters({QStringLiteral("*lcd*"), QStringLiteral("*wled*")});

    QStringList ledInterfaces = ledsDir.entryList();

    if (!ledInterfaces.isEmpty()) {
        m_dirname = LED_SYSFS_PATH + ledInterfaces.constFirst();
        return;
    }

    if (!firmware.isEmpty()) {
        m_dirname = BACKLIGHT_SYSFS_PATH + firmware.constFirst();
        return;
    }

    if (!platform.isEmpty()) {
        m_dirname = BACKLIGHT_SYSFS_PATH + platform.constFirst();
        return;
    }

    if (!raw.isEmpty()) {
        m_dirname = BACKLIGHT_SYSFS_PATH + raw.constFirst();
        return;
    }

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
            if (HAS_SYSCTL(qPrintable(QStringLiteral("hw.acpi.video.%1.brightness").arg(device))) &&
                HAS_SYSCTL(qPrintable(QStringLiteral("hw.acpi.video.%1.levels").arg(device)))) {
                m_sysctlDevice = device;
                break;
            }
        }
    }

    if (m_sysctlDevice.isEmpty()) {
        return;
    }

    size_t len;
    if (sysctlbyname(qPrintable(QStringLiteral("hw.acpi.video.%1.levels").arg(m_sysctlDevice)), nullptr, &len, nullptr, 0) != 0 ||
        len == 0) {
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
    QFile file(m_dirname + "/brightness");
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(POWERDEVIL) << "reading brightness failed with error code " << file.error() << file.errorString();
        return -1;
    }

    QTextStream stream(&file);
    stream >> brightness;
    file.close();
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
    QFile file(m_dirname + QLatin1String("/brightness"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(POWERDEVIL) << "writing brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    const int bytesWritten = file.write(QByteArray::number(brightness));
    if (bytesWritten == -1) {
        qCWarning(POWERDEVIL) << "writing brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    return true;
#endif

    return false;
}

ActionReply BacklightHelper::syspath(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported || m_dirname.isEmpty()) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData(QStringLiteral("syspath"), m_dirname);

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
    QFile file(m_dirname + QLatin1String("/max_brightness"));
    if (!file.open(QIODevice::ReadOnly)) {
        reply = ActionReply::HelperErrorReply();
//         reply.setErrorCode(file.error());
        qCWarning(POWERDEVIL) << "reading max brightness failed with error code " << file.error() << file.errorString();
        return reply;
    }

    QTextStream stream(&file);
    stream >> max_brightness;
    file.close();
#endif

    //qCDebug(POWERDEVIL) << "max brightness:" << max_brightness;

    if (max_brightness <= 0) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData(QStringLiteral("brightnessmax"), max_brightness);
    //qCDebug(POWERDEVIL) << "data contains:" << reply.data()["brightnessmax"];

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.backlighthelper", BacklightHelper)
