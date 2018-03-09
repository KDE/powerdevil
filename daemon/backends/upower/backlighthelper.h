/*  This file is part of the KDE project
 *    Copyright (C) 2010 Lukas Tinkl <ltinkl@redhat.com>
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
#include <kauth.h>

using namespace KAuth;

class BacklightHelper: public QObject
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
    void init();

    /**
     * The kernel offer from version 2.6.37 the type of the interface, and based on that
     * we can decide which interface is better for us, being the order
     * firmware-platform-raw
     */
    void initUsingBacklightType();

    /**
     * FreeBSD (and other BSDs) can control backlight via acpi_video(4)
     */
    void initUsingSysctl();

    bool m_isSupported = false;
    QString m_dirname;
    QString m_sysctlDevice;
    QList<int> m_sysctlBrightnessLevels;
};

#endif // BACKLIGHTHELPER_H
