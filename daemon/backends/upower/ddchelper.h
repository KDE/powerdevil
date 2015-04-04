/*  This file is part of the KDE project
 *    Copyright (C) 2015 Kai Uwe Broulik <kde@privat.broulik.de>
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

#ifndef DDCHELPER_H
#define DDCHELPER_H

#include <QObject>
#include <kauth.h>

using namespace KAuth;

class DdcHelper: public QObject
{
    Q_OBJECT

public:
    DdcHelper(QObject *parent = nullptr);
    virtual ~DdcHelper() = default;

public slots:
    ActionReply brightness(const QVariantMap &args);
    ActionReply brightnessmax(const QVariantMap &args);
    ActionReply setbrightness(const QVariantMap &args);

private:
    void init();

    bool m_isSupported = false;
    int m_currentBrightness = 0;
    int m_maximumBrightness = 0;
    QString m_device;
    QString m_address;

};

#endif // DDCHELPER_H
