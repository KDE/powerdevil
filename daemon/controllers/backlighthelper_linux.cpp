/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2010-2011 Lukas Tinkl <ltinkl@redhat.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#include "backlighthelper_linux.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QFile>

#include <algorithm>
#include <limits>

using namespace Qt::StringLiterals;

BacklightHelper::BacklightHelper(QObject *parent)
    : QObject(parent)
{
    init();
}

void BacklightHelper::init()
{
    m_devices = BacklightSysfsDevice::getBacklightTypeDevices();

    if (m_devices.isEmpty()) {
        qCWarning(POWERDEVIL) << "no kernel backlight interface found";
        return;
    }

    m_anim.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        // When animating to zero, it emits a value change to 0 before starting the animation...
        if (m_anim.state() == QAbstractAnimation::Running) {
            writeBrightness(value.toInt());
        }
    });

    m_isSupported = true;
}

bool BacklightHelper::writeToDevice(const QString &syspath, int brightness) const
{
    QFile file(syspath + "/brightness"_L1);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(POWERDEVIL) << "writing to device " << syspath << "/brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    const int bytesWritten = file.write(QByteArray::number(brightness));
    if (bytesWritten == -1) {
        qCWarning(POWERDEVIL) << "writing to device " << syspath << "/brightness failed with error code " << file.error() << file.errorString();
        return false;
    }

    return true;
}

ActionReply BacklightHelper::brightness(const QVariantMap &args)
{
    Q_UNUSED(args);

    if (!m_isSupported) {
        return ActionReply::HelperErrorReply();
    }

    std::optional<int> brightness = m_devices.constFirst().readBrightness();
    if (!brightness.has_value()) {
        return ActionReply::HelperErrorReply();
    }

    ActionReply reply;
    reply.addData(QStringLiteral("brightness"), *brightness);
    return reply;
}

ActionReply BacklightHelper::setbrightness(const QVariantMap &args)
{
    if (!m_isSupported) {
        return ActionReply::HelperErrorReply();
    }

    const int newBrightness = args.value(QStringLiteral("brightness")).toInt();
    const int animationDuration = args.value(QStringLiteral("animationDuration")).toInt();

    m_anim.stop();

    int currentBrightness = m_devices.constFirst().readBrightness().value_or(newBrightness);

    if (animationDuration <= 0 || currentBrightness == newBrightness) {
        writeBrightness(newBrightness);
        return ActionReply::SuccessReply();
    }

    m_anim.setDuration(animationDuration);
    m_anim.setStartValue(currentBrightness);
    m_anim.setEndValue(newBrightness);
    m_anim.start();

    return ActionReply::SuccessReply();
}

bool BacklightHelper::writeBrightness(int brightness) const
{
    if (!m_devices.isEmpty()) {
        const int first_maxbrightness = std::max(1, m_devices.constFirst().maxBrightness);
        for (const auto &device : m_devices) {
            // Some monitor brightness values are ridiculously high, and can easily overflow during computation
            const qint64 new_brightness_64 =
                static_cast<qint64>(brightness) * static_cast<qint64>(device.maxBrightness) / static_cast<qint64>(first_maxbrightness);
            // cautiously truncate it back
            const int new_brightness = static_cast<int>(std::min(static_cast<qint64>(std::numeric_limits<int>::max()), new_brightness_64));
            writeToDevice(device.syspath(), new_brightness);
        }
    }

    return true;
}

ActionReply BacklightHelper::syspath(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported || m_devices.isEmpty()) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData(QStringLiteral("syspath"), m_devices.constFirst().syspath());

    return reply;
}

ActionReply BacklightHelper::brightnessmax(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        return ActionReply::HelperErrorReply();
    }

    std::optional<int> max_brightness = m_devices.constFirst().readInt("max_brightness"_L1);
    if (!max_brightness.has_value() || *max_brightness <= 0) {
        return ActionReply::HelperErrorReply();
    }

    reply.addData(QStringLiteral("brightnessmax"), *max_brightness);
    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.backlighthelper", BacklightHelper)

#include "moc_backlighthelper_linux.cpp"
