/*
 * SPDX-FileCopyrightText: 2025 Bhushan Shah <bshah@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "wakeupsourcehelper.h"

#include <KAuth/HelperSupport>

#include <QDir>
#include <QFile>

using namespace Qt::StringLiterals;

static const QString s_wakeupCountSysFsPath = QStringLiteral("/sys/power/wakeup_count");

WakeupSourceHelper::WakeupSourceHelper(QObject *parent)
    : QObject(parent)
{
}

ActionReply WakeupSourceHelper::setwakeupcount(const QVariantMap &args)
{
    Q_UNUSED(args)

    QFile file(s_wakeupCountSysFsPath);
    // If wakeup_count can not be opened, it may mean something is running suspend already
    if (!file.open(QIODevice::ReadWrite)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to open wakeup_count, suspend maybe already in progress"));
        return reply;
    }

    // Read current wakeup count
    int wakeupCount = -1;
    QTextStream stream(&file);
    stream >> wakeupCount;

    // write same count
    if (file.write(QByteArray::number(wakeupCount)) == -1) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write wakeup_count, wakeup_count may have changed in between"));
        return reply;
    }

    return ActionReply::SuccessReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.wakeupsourcehelper", WakeupSourceHelper)

#include "moc_wakeupsourcehelper.cpp"
