/*  This file is part of the KDE project
 *    Copyright (C) 2012 Alberto Villa <avilla@FreeBSD.org>
 *    Copyright (C) 2023 Serenity Cyber Security, LLC <license@futurecrew.ru>
 *                       Author: Gleb Popov <arrowd@FreeBSD.org>
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

#ifndef BACKLIGHTHELPER_H
#define BACKLIGHTHELPER_H

#include <QObject>
#include <QVariantAnimation>

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>
#include <kauth_version.h>

using namespace KAuth;

class BacklightHelper : public QObject
{
    Q_OBJECT
public:
    explicit BacklightHelper(QObject *parent = nullptr);

public Q_SLOTS:
    ActionReply brightness(const QVariantMap &args);
    ActionReply brightnessmax(const QVariantMap &args);
    ActionReply setbrightness(const QVariantMap &args);
    ActionReply syspath(const QVariantMap &args);

private:
    int readBrightness() const;
    bool writeBrightness(int brightness) const;

    int readFromDevice(const QString &device) const;
    bool writeToDevice(const QString &device, int brightness) const;

    /**
     * FreeBSD (and other BSDs) can control backlight via acpi_video(4)
     */
    void initUsingSysctl();
    /**
     * FreeBSD 13+ has a dedicated command line utility backlight(8)
     */
    void initUsingFreeBSDBacklight();

    bool m_isSupported = false;
    QString m_sysctlDevice;
    QList<int> m_sysctlBrightnessLevels;
    QList<QPair<QString /*device path*/, int /*max brightness*/>> m_devices;

    QVariantAnimation m_anim;
};

#endif // BACKLIGHTHELPER_H
