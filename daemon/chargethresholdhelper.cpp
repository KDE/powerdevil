/*
 * Copyright 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "chargethresholdhelper.h"

#include <powerdevil_debug.h>

#include <algorithm>

#include <QDir>
#include <QFile>

static const QString s_powerSupplySysFsPath = QStringLiteral("/sys/class/power_supply");

static const QString s_chargeStartThreshold = QStringLiteral("charge_control_start_threshold");
static const QString s_chargeEndThreshold = QStringLiteral("charge_control_end_threshold");

// kept for thinkpad driver retro compat for kernels < 5.9
static const QString s_oldChargeStartThreshold = QStringLiteral("charge_start_threshold");
static const QString s_oldChargeStopThreshold = QStringLiteral("charge_stop_threshold");

ChargeThresholdHelper::ChargeThresholdHelper(QObject *parent)
    : QObject(parent)
{
}

static QStringList getBatteries()
{
    return QDir(s_powerSupplySysFsPath).entryList({QStringLiteral("BAT*")}, QDir::Dirs);
}

static QVector<int> getThresholds(const QString &which)
{
    QVector<int> thresholds;

    const QStringList batteries = getBatteries();
    for (const QString &battery : batteries) {
        QFile file(s_powerSupplySysFsPath + QLatin1Char('/') + battery + QLatin1Char('/') + which);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open" << file.fileName() << "for reading";
            continue;
        }

        int threshold = -1;
        QTextStream stream(&file);
        stream >> threshold;

        if (threshold < 0 || threshold > 100) {
            qWarning() << file.fileName() << "contains invalid threshold" << threshold;
            continue;
        }

        thresholds.append(threshold);
    }

    return thresholds;
}

static bool setThresholds(const QString &which, int threshold)
{
    const QStringList batteries = getBatteries();
    for (const QString &battery : batteries) {
        QFile file(s_powerSupplySysFsPath + QLatin1Char('/') + battery + QLatin1Char('/') + which);
        // TODO should we check the current value before writing the new one or is it clever
        // enough not to shred some chip by writing the same thing again?
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open" << file.fileName() << "for writing";
            return false;
        }

        if (file.write(QByteArray::number(threshold)) == -1) {
            qWarning() << "Failed to write threshold into" << file.fileName();
            return false;
        }
    }

    return true;
}

ActionReply ChargeThresholdHelper::getthreshold(const QVariantMap &args)
{
    Q_UNUSED(args);

    auto startThresholds = getThresholds(s_chargeStartThreshold);
    auto stopThresholds = getThresholds(s_chargeEndThreshold);

    if (startThresholds.isEmpty() || stopThresholds.isEmpty()) {
        startThresholds = getThresholds(s_oldChargeStartThreshold);
        stopThresholds = getThresholds(s_oldChargeStopThreshold);
    }

    if (startThresholds.isEmpty() || stopThresholds.isEmpty() || startThresholds.count() != stopThresholds.count()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Charge thresholds not supported"));
        return reply;
    }

    // In the rare case there are multiple batteries with varying charge thresholds, try to use something sensible
    const int startThreshold = *std::max_element(startThresholds.begin(), startThresholds.end());
    const int stopThreshold = *std::min_element(stopThresholds.begin(), stopThresholds.end());

    ActionReply reply;
    reply.setData({
        {QStringLiteral("chargeStartThreshold"), startThreshold},
        {QStringLiteral("chargeStopThreshold"), stopThreshold}
    });
    return reply;
}

ActionReply ChargeThresholdHelper::setthreshold(const QVariantMap &args)
{
    const int startThreshold = args.value(QStringLiteral("chargeStartThreshold")).toInt();
    const int stopThreshold = args.value(QStringLiteral("chargeStopThreshold")).toInt();

    if (startThreshold < 0
            || startThreshold > 100
            || stopThreshold < 0
            || stopThreshold > 100
            || startThreshold > stopThreshold) {
        auto reply = ActionReply::HelperErrorReply(); // is there an "invalid arguments" error?
        reply.setErrorDescription(QStringLiteral("Invalid thresholds provided"));
        return reply;
    }

    if (!(setThresholds(s_chargeStartThreshold, startThreshold) || setThresholds(s_oldChargeStartThreshold, startThreshold))) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write start charge threshold"));
        return reply;
    }

    if (!(setThresholds(s_chargeEndThreshold, stopThreshold) || setThresholds(s_oldChargeStopThreshold, stopThreshold))) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write stop charge threshold"));
        return reply;
    }

    return ActionReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.chargethresholdhelper", ChargeThresholdHelper)
