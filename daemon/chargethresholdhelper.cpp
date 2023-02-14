/*a
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

#include <KAuth/HelperSupport>

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
    const QStringList power_supplies = QDir(s_powerSupplySysFsPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList batteries;

    for (const QString &psu : power_supplies) {
        QDir psuDir(s_powerSupplySysFsPath + QLatin1Char('/') + psu);
        QFile file(psuDir.filePath("type"));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QString psu_type;
        QTextStream stream(&file);
        stream >> psu_type;

        if (psu_type.trimmed() != QLatin1String("Battery")) {
            continue; // Not a battery, skip
        }

        if (!psuDir.exists(s_chargeStartThreshold) && !psuDir.exists(s_oldChargeStartThreshold)
            && !psuDir.exists(s_chargeEndThreshold) && !psuDir.exists(s_oldChargeStopThreshold)) {
            continue; // No charge thresholds, skip
        }

        batteries.append(psu);
    }

    return batteries;
}

static QVector<int> getThresholds(const QString &which)
{
    QVector<int> thresholds;

    const QStringList batteries = getBatteries();
    for (const QString &battery : batteries) {
        QFile file(s_powerSupplySysFsPath + QLatin1Char('/') + battery + QLatin1Char('/') + which);
        if (!file.open(QIODevice::ReadOnly)) {
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

    if (startThresholds.isEmpty() && stopThresholds.isEmpty()) {
        startThresholds = getThresholds(s_oldChargeStartThreshold);
        stopThresholds = getThresholds(s_oldChargeStopThreshold);
    }

    if (startThresholds.isEmpty() && stopThresholds.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Charge thresholds are not supported by the kernel for this hardware"));
        return reply;
    }

    if (!startThresholds.isEmpty() && !stopThresholds.isEmpty() && startThresholds.count() != stopThresholds.count()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Charge thresholds are not supported by the kernel for this hardware"));
        return reply;
    }

    // In the rare case there are multiple batteries with varying charge thresholds, try to use something sensible
    const int startThreshold = !startThresholds.empty() ? *std::max_element(startThresholds.begin(), startThresholds.end()) : -1;
    const int stopThreshold = !stopThresholds.empty() ? *std::min_element(stopThresholds.begin(), stopThresholds.end()) : -1;

    ActionReply reply;
    reply.setData({
        {QStringLiteral("chargeStartThreshold"), startThreshold},
        {QStringLiteral("chargeStopThreshold"), stopThreshold}
    });
    return reply;
}

ActionReply ChargeThresholdHelper::setthreshold(const QVariantMap &args)
{
    bool hasStartThreshold;
    const int startThreshold = args.value(QStringLiteral("chargeStartThreshold"), -1).toInt(&hasStartThreshold);
    hasStartThreshold &= startThreshold != -1;

    bool hasStopThreshold;
    const int stopThreshold = args.value(QStringLiteral("chargeStopThreshold"), -1).toInt(&hasStopThreshold);
    hasStopThreshold &= stopThreshold != -1;

    if ((hasStartThreshold && (startThreshold < 0|| startThreshold > 100))
            || (hasStopThreshold && (stopThreshold < 0 || stopThreshold > 100))
            || (hasStartThreshold && hasStopThreshold && startThreshold > stopThreshold)
            || (!hasStartThreshold && !hasStopThreshold)) {
        auto reply = ActionReply::HelperErrorReply(); // is there an "invalid arguments" error?
        reply.setErrorDescription(QStringLiteral("Invalid thresholds provided"));
        return reply;
    }

    if (hasStartThreshold && !(setThresholds(s_chargeStartThreshold, startThreshold) || setThresholds(s_oldChargeStartThreshold, startThreshold))) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write start charge threshold"));
        return reply;
    }

    if (hasStopThreshold && !(setThresholds(s_chargeEndThreshold, stopThreshold) || setThresholds(s_oldChargeStopThreshold, stopThreshold))) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write stop charge threshold"));
        return reply;
    }

    return ActionReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.chargethresholdhelper", ChargeThresholdHelper)
