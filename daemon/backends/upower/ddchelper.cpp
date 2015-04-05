/*
 *   Copyright 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ddchelper.h"

#include <powerdevil_debug.h>

#include <QDebug>
#include <QProcess>

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
            continue;
        }

        if (line.startsWith(QLatin1String("> id=brightness"))) {
            // first get the address in the first line which has id, name, address, delay and type in it
            {
                const QStringList params = line.split(QLatin1Char(','));
                for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                    const QString param = (*it).simplified();
                    const QStringList pair = param.split(QLatin1Char('='));
                    if (pair.count() == 2 && pair.first() == QLatin1String("address")) {
                        // found it
                        m_address = pair.last();
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
                    qCWarning(POWERDEVIL) << "We got an address for our monitor" << m_address << "but it is not supported";
                    return;
                }

                const QStringList params = line.split(QLatin1Char(','));
                for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                    const QStringList pair = (*it).simplified().split('=');
                    if (pair.count() != 2) {
                        continue;
                    }

                    if (pair.first() == QLatin1String("value")) {
                        m_currentBrightness = pair.last().toInt();
                    } else if (pair.first() == QLatin1String("maximum")) {
                        m_maximumBrightness = pair.last().toInt();
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

    // We don't query the actual brightness again since PowerDevil just queries it once anyway

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

    // don't care whether it worked or not

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
