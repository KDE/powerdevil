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

#include <sys/utsname.h>

#ifdef Q_OS_FREEBSD
#define USE_SYSCTL
#endif

#ifdef USE_SYSCTL
#include <sys/types.h>
#include <sys/sysctl.h>

#define HAS_SYSCTL(n) (sysctlbyname(n, NULL, NULL, NULL, 0) == 0)
#endif

#define PREFIX "/sys/class/backlight/"

BacklightHelper::BacklightHelper(QObject *parent) : QObject(parent)
{
    init();
}

void BacklightHelper::init()
{
    qDebug() << "BACKLIGHT HELPER INIT";
    initUsingBacklightType();
    qDebug() << "dir name?" << m_dirname << m_dirname.isEmpty();

    if (m_dirname.isEmpty()) {
        initUsingSysctl();

        qDebug() << m_sysctlDevice << m_sysctlDevice.isEmpty() << m_sysctlBrightnessLevels << m_sysctlBrightnessLevels.isEmpty();

        if (m_sysctlDevice.isEmpty() || m_sysctlBrightnessLevels.isEmpty()) {
            qCWarning(POWERDEVIL) << "no kernel backlight interface found";
            return;
        }
    }

    qDebug() << "supported";

    m_isSupported = true;
}

void BacklightHelper::initUsingBacklightType()
{
    QDir dir(PREFIX);
    dir.setFilter(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
    dir.setSorting(QDir::Name | QDir::Reversed);// Reverse is needed to priorize acpi_video1 over 0

    QStringList interfaces = dir.entryList();

    if (interfaces.isEmpty()) {
        return;
    }

    QFile file;
    QByteArray buffer;
    QStringList firmware, platform, raw;

    foreach(const QString & interface, interfaces) {
        file.setFileName(PREFIX + interface + "/type");
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

    if (!firmware.isEmpty()) {
        m_dirname = PREFIX + firmware.first();
        return;
    }

    if (!platform.isEmpty()) {
        m_dirname = PREFIX + platform.first();
        return;
    }

    if (!raw.isEmpty()) {
        m_dirname = PREFIX + raw.first();
        return;
    }
}

void BacklightHelper::initUsingSysctl()
{
    qDebug() << "Use sysctl";
#ifdef USE_SYSCTL
    /*
     * lcd0 is, in theory, the correct device, but some vendors have custom ACPI implementations
     * which cannot be interpreted. In that case, devices should be reported as "out", but
     * FreeBSD doesn't care (yet), so they can appear as any other type. Let's search for the first
     * device with brightness management, then.
     */
    QStringList types;
    types << "lcd" << "out" << "crt" << "tv" << "ext";
    foreach (const QString &type, types) {
        for (int i = 0; m_sysctlDevice.isEmpty(); i++) {
            QString device = QString("%1%2").arg(type, QString::number(i));
            // We don't care about the value, we only want the sysctl to be there.
            if (!HAS_SYSCTL(qPrintable(QString("hw.acpi.video.%1.active").arg(device)))) {
                break;
            }
            if (HAS_SYSCTL(qPrintable(QString("hw.acpi.video.%1.brightness").arg(device))) &&
                HAS_SYSCTL(qPrintable(QString("hw.acpi.video.%1.levels").arg(device)))) {
                m_sysctlDevice = device;
                break;
            }
        }
    }

    if (m_sysctlDevice.isEmpty()) {
        return;
    }

    size_t len;
    if (sysctlbyname(qPrintable(QString("hw.acpi.video.%1.levels").arg(m_sysctlDevice)), NULL, &len, NULL, 0) != 0 ||
        len == 0) {
        return;
    }
    int *levels = (int *)malloc(len);
    if (!levels) {
        return;
    }
    if (sysctlbyname(qPrintable(QString("hw.acpi.video.%1.levels").arg(m_sysctlDevice)), levels, &len, NULL, 0) != 0) {
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
    qSort(m_sysctlBrightnessLevels.begin(), m_sysctlBrightnessLevels.end());
#endif
}

ActionReply BacklightHelper::brightness(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    // current brightness
    int brightness;

#ifdef USE_SYSCTL
    size_t len = sizeof(int);
    if (sysctlbyname(qPrintable(QString("hw.acpi.video.%1.brightness").arg(m_sysctlDevice)), &brightness, &len, NULL, 0) != 0) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }
#else
    QFile file(m_dirname + "/brightness");
    if (!file.open(QIODevice::ReadOnly)) {
        reply = ActionReply(ActionReply::HelperErrorType);
        reply.setErrorDescription(i18n("Can't open file"));
        qCWarning(POWERDEVIL) << "reading brightness failed with error code " << file.error() << file.errorString();
        return reply;
    }

    QTextStream stream(&file);
    stream >> brightness;
    file.close();
#endif

    //qCDebug(POWERDEVIL) << "brightness:" << brightness;
    reply.addData("brightness", brightness);
    //qCDebug(POWERDEVIL) << "data contains:" << reply.data()["brightness"];

    return reply;
}

ActionReply BacklightHelper::setbrightness(const QVariantMap &args)
{
    ActionReply reply;

    int actual_brightness = args.value(QStringLiteral("brightness")).toInt();

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    //qCDebug(POWERDEVIL) << "setting brightness:" << actual_brightness;

#ifdef USE_SYSCTL
    int actual_level = -1;
    int d1 = 101;
    // Search for the nearest level.
    foreach (int level, m_sysctlBrightnessLevels) {
        int d2 = qAbs(level - actual_brightness);
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
    if (sysctlbyname(qPrintable(QString("hw.acpi.video.%1.brightness").arg(m_sysctlDevice)), NULL, NULL, &actual_level, len) != 0) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }
#else
    QFile file(m_dirname + "/brightness");
    if (!file.open(QIODevice::WriteOnly)) {
        reply = ActionReply::HelperErrorReply();
//         reply.setErrorCode(ActionReply::ActionReply::UserCancelledError);
        qCWarning(POWERDEVIL) << "writing brightness failed with error code " << file.error() << file.errorString();
        return reply;
    }

    int result = file.write(QByteArray::number(actual_brightness));
    file.close();

    if (result == -1) {
        reply = ActionReply::HelperErrorReply();
//         reply.setErrorCode(file.error());
        qCWarning(POWERDEVIL) << "writing brightness failed with error code " << file.error() << file.errorString();
    }
#endif
    return reply;
}

ActionReply BacklightHelper::syspath(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported || m_dirname.isEmpty()) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData("syspath", m_dirname);

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
    QFile file(m_dirname + "/max_brightness");
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

    reply.addData("brightnessmax", max_brightness);
    //qCDebug(POWERDEVIL) << "data contains:" << reply.data()["brightnessmax"];

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.backlighthelper", BacklightHelper)
