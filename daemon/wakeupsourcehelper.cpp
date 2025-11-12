/*
 * SPDX-FileCopyrightText: 2025 Bhushan Shah <bshah@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "wakeupsourcehelper.h"

#include <KAuth/HelperSupport>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

static const QString s_wakeupCountSysFsPath = QStringLiteral("/sys/power/wakeup_count");
static const QString s_memSleepModeSysFsPath = QStringLiteral("/sys/power/mem_sleep");

WakeupSourceHelper::WakeupSourceHelper(QObject *parent)
    : QObject(parent)
{
}

ActionReply WakeupSourceHelper::setwakeupcount(const QVariantMap &args)
{
    Q_UNUSED(args)

    // First check if current suspend mode is s2idle, then only proceed further
    QFile memSleepFile(s_memSleepModeSysFsPath);
    if (!memSleepFile.open(QIODevice::ReadOnly)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to get current sleep mode, skipping wakeup source helper"));
        return reply;
    }

    QString sleepMode;
    QTextStream memSleepStream(&memSleepFile);

    // text will be in form of "s2idle [deep]", we want to check current mode.
    QRegularExpression re(uR"(\[(\w+)\])"_s);
    QRegularExpressionMatch match = re.match(memSleepStream.readLine());
    if (match.hasMatch()) {
        sleepMode = match.captured(1);
    }

    if (sleepMode != QLatin1StringView("s2idle")) {
        // Since currently there are platform bugs with deep mode
        // let's skip writing wakeup count.
        return ActionReply::SuccessReply();
    }

    QFile wakeupFile(s_wakeupCountSysFsPath);
    // If wakeup_count can not be opened, it may mean something is running suspend already
    if (!wakeupFile.open(QIODevice::ReadWrite)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to open wakeup_count, suspend maybe already in progress"));
        return reply;
    }

    // Read current wakeup count
    int wakeupCount = -1;
    QTextStream stream(&wakeupFile);
    stream >> wakeupCount;

    // write same count
    if (wakeupFile.write(QByteArray::number(wakeupCount)) == -1) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write wakeup_count, wakeup_count may have changed in between"));
        return reply;
    }

    return ActionReply::SuccessReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.wakeupsourcehelper", WakeupSourceHelper)

#include "moc_wakeupsourcehelper.cpp"
