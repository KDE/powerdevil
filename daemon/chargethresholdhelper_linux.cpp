/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 * SPDX-FileCopyrightText: 2023 Fabian Arndt <fabian.arndt@root-core.net>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "chargethresholdhelper.h"

#include <powerdevil_debug.h>

#include <KAuth/HelperSupport>

#include <algorithm>

#include <QDir>
#include <QFile>

using namespace Qt::StringLiterals;

static const QString s_powerSupplySysFsPath = QStringLiteral("/sys/class/power_supply");
static const QString s_conservationModeSysFsPath = QStringLiteral("/sys/bus/platform/drivers/ideapad_acpi/VPC2004:00/conservation_mode");

static const QString s_chargeStartThreshold = QStringLiteral("charge_control_start_threshold");
static const QString s_chargeEndThreshold = QStringLiteral("charge_control_end_threshold");
static const QString s_chargeTypesFilename = QStringLiteral("charge_types");

// Charge types supported by this helper, the device & kernel may support more than these. Docs:
// https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-power - see "charge_type" and "charge_types"
static const QByteArray s_chargeTypeStandard = QByteArrayLiteral("Standard");
static const QByteArray s_chargeTypeCustom = QByteArrayLiteral("Custom");

ChargeThresholdHelper::ChargeThresholdHelper(QObject *parent)
    : QObject(parent)
{
}

static QStringList getBatteries()
{
    const QStringList power_supplies = QDir(s_powerSupplySysFsPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList batteries;

    for (const QString &psu : power_supplies) {
        QDir psuDir(QString(s_powerSupplySysFsPath + u'/' + psu));
        QFile file(psuDir.filePath(u"type"_s));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QString psu_type;
        QTextStream stream(&file);
        stream >> psu_type;

        if (psu_type.trimmed() != QLatin1String("Battery")) {
            continue; // Not a battery, skip
        }

        if (!psuDir.exists(s_chargeStartThreshold) && !psuDir.exists(s_chargeEndThreshold)) {
            continue; // No charge thresholds, skip
        }

        batteries.append(psu);
    }

    return batteries;
}

static int getThreshold(const QString &battery, const QString &which)
{
    QFile file(s_powerSupplySysFsPath + QLatin1Char('/') + battery + QLatin1Char('/') + which);

    // We don't need write access here, but if we can't write then it's better to mark this
    // threshold as unsupported so we won't try to write to it later. To the user, a read-only
    // value that's fixed by the firmware won't be very interesting anyway.
    if (!file.open(QIODevice::ReadWrite)) {
        return -1;
    }

    int threshold = -1;
    QTextStream stream(&file);
    stream >> threshold;

    if (threshold < 0 || threshold > 100) {
        qWarning() << file.fileName() << "contains invalid threshold" << threshold;
        return -1;
    }

    return threshold;
}

static QMap<QString, int> getThresholds(const QString &which)
{
    QMap<QString, int> thresholds;

    const QStringList batteries = getBatteries();
    for (const QString &battery : batteries) {
        if (int threshold = getThreshold(battery, which); threshold != -1) {
            thresholds.insert(battery, threshold);
        }
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

static bool setChargeType(const QByteArray &chargeType)
{
    const QStringList batteries = getBatteries();
    for (const QString &battery : batteries) {
        QFile file(s_powerSupplySysFsPath + QLatin1Char('/') + battery + QLatin1Char('/') + s_chargeTypesFilename);
        if (!file.exists()) {
            // some drivers support start/end threshold without charge type selection - ignore those
            continue;
        }

        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open" << file.fileName() << "for writing";
            return false;
        }

        if (file.write(chargeType) == -1) {
            qWarning() << "Failed to charge type into" << file.fileName();
            return false;
        }
    }

    return true;
}

ActionReply ChargeThresholdHelper::getthreshold(const QVariantMap &args)
{
    Q_UNUSED(args);

    QMap<QString, int> stopThresholds = getThresholds(s_chargeEndThreshold);

    // In the rare case there are multiple batteries with varying charge thresholds, try to use something sensible
    const auto stopThresholdIt = std::ranges::min_element(std::as_const(stopThresholds));
    if (stopThresholdIt == stopThresholds.cend()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Charge thresholds are not supported by the kernel for this hardware"));
        return reply;
    }

    const int stopThreshold = *stopThresholdIt;
    const int startThreshold = getThreshold(stopThresholdIt.key(), s_chargeStartThreshold);

    ActionReply reply;
    reply.setData({
        {u"chargeStartThreshold"_s, startThreshold},
        {u"chargeStopThreshold"_s, stopThreshold},
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

    QByteArray chargeType = args.value(QStringLiteral("chargeType"), QString()).toByteArray();
    bool requiresCustomThresholds = chargeType.isEmpty() || chargeType == s_chargeTypeCustom;
    bool hasCustomThresholds = hasStartThreshold || hasStopThreshold;

    if (requiresCustomThresholds && !hasCustomThresholds) {
        auto reply = ActionReply::HelperErrorReply(); // is there an "invalid arguments" error?
        reply.setErrorDescription(QStringLiteral("Thresholds required, but not provided"));
        return reply;
    }

    if (requiresCustomThresholds) {
        bool thresholdsInvalid = //
            (hasStartThreshold && (startThreshold < 0 || startThreshold > 100)) //
            || (hasStopThreshold && (stopThreshold < 0 || stopThreshold > 100)) //
            || (hasStartThreshold && hasStopThreshold && startThreshold > stopThreshold);

        if (thresholdsInvalid) {
            auto reply = ActionReply::HelperErrorReply(); // is there an "invalid arguments" error?
            reply.setErrorDescription(QStringLiteral("Invalid thresholds provided"));
            return reply;
        }

        if (hasStartThreshold && !setThresholds(s_chargeStartThreshold, startThreshold)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to write start charge threshold"));
            return reply;
        }

        if (hasStopThreshold && !setThresholds(s_chargeEndThreshold, stopThreshold)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to write stop charge threshold"));
            return reply;
        }
    }

    if (!chargeType.isEmpty()) {
        if (!setChargeType(chargeType)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to write start charge type"));
            return reply;
        }
    } else if (requiresCustomThresholds) { // best effort automatic assignment, no errors
        if ((startThreshold == 0 || startThreshold == 100) && (stopThreshold == 0 || stopThreshold == 100)) {
            setChargeType(s_chargeTypeStandard);
        } else {
            setChargeType(s_chargeTypeCustom);
        }
    }

    return ActionReply();
}

// This handles Lenovo's "battery conservation mode" that limits the threshold to a fixed value
// This value differs from model to model (most 55-60%, others ~80%) and is unknown (last checked in Kernel 6.3)
ActionReply ChargeThresholdHelper::getconservationmode(const QVariantMap &args)
{
    Q_UNUSED(args);

    QFile file(s_conservationModeSysFsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Battery conservation mode is not supported"));
        return reply;
    }

    int state = 0;
    QTextStream stream(&file);
    stream >> state;

    if (state < 0 || state > 1) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Battery conservation mode is in an invalid state"));
        return reply;
    }

    ActionReply reply;
    reply.setData({{QStringLiteral("batteryConservationModeEnabled"), !!state}});
    return reply;
}

ActionReply ChargeThresholdHelper::setconservationmode(const QVariantMap &args)
{
    const bool enabled = args.value(QStringLiteral("batteryConservationModeEnabled"), 0).toBool();
    QFile file(s_conservationModeSysFsPath);

    if (!file.open(QIODevice::WriteOnly)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to open battery conservation mode file"));
        return reply;
    }

    if (file.write(QByteArray::number(enabled)) == -1) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to set battery conservation mode"));
        return reply;
    }

    return ActionReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.chargethresholdhelper", ChargeThresholdHelper)

#include "moc_chargethresholdhelper.cpp"
