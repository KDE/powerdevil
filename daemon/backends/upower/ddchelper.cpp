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

#include "ddchelper.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QProcess>

#include <KLocalizedString>

DdcHelper::DdcHelper(QObject *parent) : QObject(parent)
{
    init();
}

void DdcHelper::init()
{
    QProcess ddcProcess;
    ddcProcess.start(QStringLiteral("ddccontrol"), {QStringLiteral("-p")});
    if (!ddcProcess.waitForFinished()) {
        qCDebug(POWERDEVIL) << "Failed to run ddccontrol" << ddcProcess.errorString();
        return;
    }

    // see through the output and find the brightness section where the address and values lie
    while (ddcProcess.canReadLine()) {
        const QString line = QString::fromLocal8Bit(ddcProcess.readLine()).simplified();
        if (line.startsWith(QLatin1String("- Device:"))) {
            m_device = line.mid(10);
            qCDebug(POWERDEVIL) << "Found monitor at device" << m_device;
            continue;
        }

        if (line.startsWith(QLatin1String("> id=brightness"))) {
            // first get the address in the first line which has id, name, address, delay and type in it
            {
                const QStringList params = line.split(QLatin1Char(','));
                for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                    const QString param = (*it).simplified();
                    const QStringList pair = param.split(QLatin1Char('='));
                    if (pair.first() == QLatin1String("address")) {
                        // found it
                        m_address = pair.last();
                        qCDebug(POWERDEVIL) << "Found backlight address" << m_address;
                        break;
                    }
                }

                if (!ddcProcess.canReadLine()) { // just to be sure
                    qCWarning(POWERDEVIL) << "Got first row of brightness info but no second one, something's wrong here";
                    return;
                }
            }

            {
                const QString line = QString::fromLocal8Bit(ddcProcess.readLine()).simplified();
                if (!line.startsWith(QLatin1String("supported"))) {
                    qCDebug(POWERDEVIL) << "We got an address for our monitor" << m_address << "but it is not supported";
                    return;
                }

                const QStringList params = line.split(QLatin1Char(','));
                for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                    const QStringList pair = (*it).simplified().split('=');
                    if (pair.first() == QLatin1String("value")) {
                        m_currentBrightness = pair.last().toInt();
                        qCDebug(POWERDEVIL) << "Monitor current brightness" << m_address << "is" << m_currentBrightness;
                    } else if (pair.first() == QLatin1String("maximum")) {
                        m_maximumBrightness = pair.last().toInt();
                        qCDebug(POWERDEVIL) << "Monitor maximum brightness" << m_address << "is" << m_maximumBrightness;
                    }
                }
            }

            break;
        }
    }

    if (m_device.isEmpty() || m_address.isEmpty() || !m_maximumBrightness) {
        qCDebug(POWERDEVIL) << "DDC is not supported, our device is" << m_device << ", our address is" << m_address << ", have a maximum of" << m_maximumBrightness;
        return;
    }

    qCDebug(POWERDEVIL) << "We now have the full DD Control!!!";

    m_isSupported = true;
}

ActionReply DdcHelper::brightness(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    // TODO query the actual brightness
    // but I think we don't ever query that in PowerDevil again anyway

    reply.addData("brightness", m_currentBrightness);

    return reply;
}

ActionReply DdcHelper::setbrightness(const QVariantMap &args)
{
    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    const int newBrightness = args.value(QStringLiteral("brightness")).toInt();

    QProcess ddcProcess;
    ddcProcess.execute(QStringLiteral("ddccontrol"), {
        QStringLiteral("-r"), m_address,
        QStringLiteral("-w"), QString::number(newBrightness),
        m_device
    });

    if (!ddcProcess.waitForFinished()) { // somehow always says it failed
        reply = ActionReply::HelperErrorReply();
        qCWarning(POWERDEVIL) << "Failed to execute ddccontrol for setting brightness" << ddcProcess.errorString();
        return reply;
    }

    return reply;
}

ActionReply DdcHelper::brightnessmax(const QVariantMap &args)
{
    Q_UNUSED(args);

    ActionReply reply;

    if (!m_isSupported) {
        reply = ActionReply::HelperErrorReply();
        return reply;
    }

    reply.addData("brightnessmax", m_maximumBrightness);

    return reply;
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.ddchelper", DdcHelper)
