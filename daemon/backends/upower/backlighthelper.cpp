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

#define PREFIX "/sys/class/backlight/"

BacklightHelper::BacklightHelper(QObject * parent)
    : QObject(parent), m_isSupported(false)
{
    init();
}

void BacklightHelper::init()
{
    // find the first existing device with backlight support
    QStringList interfaces;
    interfaces << "nv_backlight" << "intel_backlight" << "radeon_bl" << "mbp_backlight"
               << "asus_laptop" << "toshiba" << "eeepc" << "thinkpad_screen" << "acpi_video1"
               << "acpi_video0" << "apple_backlight" << "fujitsu-laptop" << "samsung"
               << "nvidia_backlight" << "dell_backlight" << "sony"
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

    if (m_dirname.isEmpty()) {
        qWarning() << "no kernel backlight interface found";
        return;
    }

    m_isSupported = true;
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
