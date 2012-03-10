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

#include <QtCore/QDir>
#include <QtCore/QDebug>

#include <sys/utsname.h>

#define PREFIX "/sys/class/backlight/"

BacklightHelper::BacklightHelper(QObject * parent)
    : QObject(parent), m_isSupported(false)
{
    init();
}

void BacklightHelper::init()
{

    if (useWhitelistInit()) {
        initUsingWhitelist();
    } else {
        initUsingBacklightType();
    }

    if (m_dirname.isEmpty()) {
        qWarning() << "no kernel backlight interface found";
        return;
    }

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
            firmware.append(buffer);
        } else if(buffer == "platform") {
            platform.append(buffer);
        } else if (buffer == "raw") {
            raw.append(buffer);
        } else {
            qWarning() << "Interface type not handled" << buffer;
        }

        file.close();
    }

    if (!firmware.isEmpty()) {
        m_dirname = firmware.first();
        return;
    }

    if (!platform.isEmpty()) {
        m_dirname = platform.first();
        return;
    }

    if (!raw.isEmpty()) {
        m_dirname = raw.first();
        return;
    }
}


void BacklightHelper::initUsingWhitelist()
{
    QStringList interfaces;
    interfaces << "nv_backlight" << "radeon_bl" << "mbp_backlight" << "asus_laptop"
               << "toshiba" << "eeepc" << "thinkpad_screen" << "acpi_video1" << "acpi_video0"
               << "intel_backlight" << "apple_backlight" << "fujitsu-laptop" << "samsung"
               << "nvidia_backlight" << "dell_backlight" << "sony" << "pwm-backlight"
               ;

    QDir dir;
    foreach (const QString & interface, interfaces) {
        dir.setPath(PREFIX + interface);
        //qDebug() << "searching dir:" << dir;
        if (dir.exists()) {
            m_dirname = dir.path();
            //qDebug() << "kernel backlight support found in" << m_dirname;
            break;
        }
    }

    //If none of our whitelisted interface is available, get the first one  (if any)
    if (m_dirname.isEmpty()) {
        dir.setPath(PREFIX);
        dir.setFilter(QDir::AllDirs | QDir::NoDot | QDir::NoDotDot | QDir::NoDotAndDotDot | QDir::Readable);
        QStringList dirList = dir.entryList();
        if (!dirList.isEmpty()) {
            m_dirname = dirList.first();
        }
    }
}

bool BacklightHelper::useWhitelistInit()
{
    struct utsname uts;
    uname(&uts);

    int major, minor, patch, result;
    result = sscanf(uts.release, "%d.%d", &major, &minor);

    if (result != 2) {
        return true; // Malformed version
    }

    if (major == 3) {
        return false; //Kernel 3, we want type based init
    }

    result = sscanf(uts.release, "%d.%d.%d", &major, &minor, &patch);

    if (result != 3) {
        return true; // Malformed version
    }

    if (patch < 37) {
        return true; //Minor than 2.6.37, use whiteList based
    }

    return false;//Use Type based interafce
}

ActionReply BacklightHelper::brightness(const QVariantMap & args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply;
        return reply;
    }

    // current brightness
    QFile file(m_dirname + "/brightness");
    if (!file.open(QIODevice::ReadOnly)) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(file.error());
        qWarning() << "reading brightness failed with error code " << file.error() << file.errorString();
        return reply;
    }

    QTextStream stream(&file);
    int brightness;
    stream >> brightness;
    //qDebug() << "brightness:" << brightness;
    file.close();

    reply.addData("brightness", brightness * 100 / maxBrightness());
    //qDebug() << "data contains:" << reply.data()["brightness"];

    return reply;
}

ActionReply BacklightHelper::setbrightness(const QVariantMap & args)
{
    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply;
        return reply;
    }

    QFile file(m_dirname + "/brightness");
    if (!file.open(QIODevice::WriteOnly)) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(file.error());
        qWarning() << "writing brightness failed with error code " << file.error() << file.errorString();
        return reply;
    }

    int actual_brightness = qRound(args["brightness"].toFloat() * maxBrightness() / 100);
    //qDebug() << "setting brightness:" << actual_brightness;
    int result = file.write(QByteArray::number(actual_brightness));
    file.close();

    if (result == -1) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(file.error());
        qWarning() << "writing brightness failed with error code " << file.error() << file.errorString();
    }

    return reply;
}

int BacklightHelper::maxBrightness() const
{
    // maximum brightness
    QFile file(m_dirname + "/max_brightness");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "reading max brightness failed with error code " << file.error() << file.errorString();
        return -1; // some non-zero value
    }

    QTextStream stream(&file);
    int max_brightness;
    stream >> max_brightness;
    //qDebug() << "max brightness:" << max_brightness;
    file.close();

    return max_brightness;
}

KDE4_AUTH_HELPER_MAIN("org.kde.powerdevil.backlighthelper", BacklightHelper)
