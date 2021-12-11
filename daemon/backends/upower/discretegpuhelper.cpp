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

#include "discretegpuhelper.h"

#include <powerdevil_debug.h>

#include <QFile>

#define SWITCHEROO_SYSFS_PATH "/sys/kernel/debug/vgaswitcheroo/switch"

DiscreteGpuHelper::DiscreteGpuHelper(QObject *parent)
    : QObject(parent)
{
}

ActionReply DiscreteGpuHelper::hasdualgpu(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;
    reply.addData(QStringLiteral("hasdualgpu"), QFile::exists(SWITCHEROO_SYSFS_PATH));
    // qCDebug(POWERDEVIL) << "data contains:" << reply.data()["hasdualgpu"];

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.discretegpuhelper", DiscreteGpuHelper)

