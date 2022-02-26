/*  This file is part of the KDE project
 *    Copyright (C) 2016 Jan Grulich <jgrulich@redhat.com>
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

#ifndef DISCRETEGPUHELPER_H
#define DISCRETEGPUHELPER_H

#include <QObject>
#include <kauth_version.h>
#if KAUTH_VERSION >= QT_VERSION_CHECK(5, 92, 0)
#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>
#else
#include <KAuth>
#endif

using namespace KAuth;

class DiscreteGpuHelper : public QObject
{
    Q_OBJECT
public:
    explicit DiscreteGpuHelper(QObject *parent = nullptr);

public Q_SLOTS:
    ActionReply hasdualgpu(const QVariantMap &args);

};

#endif // DISCRETEGPUHELPER_H

