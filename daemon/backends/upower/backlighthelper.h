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
    BacklightHelper(QObject * parent = 0);

public slots:
    ActionReply brightness(const QVariantMap & args);
    ActionReply setbrightness(const QVariantMap & args);

private:
    void init();
    /**
     * For older kernels that doesn't indicate which type the interface use we have
     * a whitelsit based on the feedback given by our users, if the interface is not
     * within our whitelist we will look for a random one within the backlight folder
     */
    void initUsingWhitelist();

    /**
     * The kernel offer from version 2.6.37 the type of the interface, and based on that
     * we can decide which interface is better for us, being the order
     * firmware-platform-raw
     */
    void initUsingBacklightType();

    /**
     * If Kernel older than 2.6.37 use whitelsit, otherwise use backlight/type
     * @see https://bugs.kde.org/show_bug.cgi?id=288180
     */
    bool useWhitelistInit();
    int maxBrightness() const;
    bool m_isSupported;
    QString m_dirname;
};

#endif // BACKLIGHTHELPER_H
