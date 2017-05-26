/*  This file is part of the KDE project
 *    Copyright (C) 2017 Bhushan Shah <bshah@kde.org>
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

#include <QFileInfo>

#include <KAuthAction>
#include <KAuthExecuteJob>

#include <powerdevil_debug.h>
#include "sysfsbrightness.h"

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

SysfsBrightness::SysfsBrightness()
{
}

void SysfsBrightness::detect()
{
    KAuth::Action brightnessAction("org.kde.powerdevil.backlighthelper.brightness");
    brightnessAction.setHelperId(HELPER_ID);
    KAuth::ExecuteJob *brightnessJob = brightnessAction.execute();
    connect(brightnessJob, &KJob::result, this,
        [this, brightnessJob]  {
            if (brightnessJob->error()) {
                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightness failed";
                qCDebug(POWERDEVIL) << brightnessJob->errorText();
                return;
            }
            m_brightness = brightnessJob->data()["brightness"].toInt();

            KAuth::Action brightnessMaxAction("org.kde.powerdevil.backlighthelper.brightnessmax");
            brightnessMaxAction.setHelperId(HELPER_ID);
            KAuth::ExecuteJob *brightnessMaxJob = brightnessMaxAction.execute();
            connect(brightnessMaxJob, &KJob::result, this,
                [this, brightnessMaxJob] {
                    if (brightnessMaxJob->error()) {
                        qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.brightnessmax failed";
                        qCDebug(POWERDEVIL) << brightnessMaxJob->errorText();
                    } else {
                        m_brightnessMax = brightnessMaxJob->data()["brightnessmax"].toInt();
                    }

                    KAuth::Action syspathAction("org.kde.powerdevil.backlighthelper.syspath");
                    syspathAction.setHelperId(HELPER_ID);
                    KAuth::ExecuteJob* syspathJob = syspathAction.execute();
                    connect(syspathJob, &KJob::result, this,
                        [this, syspathJob] {
                            if (syspathJob->error()) {
                                qCWarning(POWERDEVIL) << "org.kde.powerdevil.backlighthelper.syspath failed";
                                qCDebug(POWERDEVIL) << syspathJob->errorText();
                                return;
                            }
                            m_syspath = syspathJob->data()["syspath"].toString();
                            m_syspath = QFileInfo(m_syspath).readLink();

                            m_isLedBrightnessControl = m_syspath.contains(QLatin1String("/leds/"));
                            //TODO: handle the brightness changing from other sources
                            //if (!m_isLedBrightnessControl) {
                            //    UdevQt::Client *client =  new UdevQt::Client(QStringList("backlight"), this);
                            //    connect(client, SIGNAL(deviceChanged(UdevQt::Device)), SLOT(onDeviceChanged(UdevQt::Device)));
                            //}

                            m_isSupported = (m_brightnessMax > 0);
                            return;
                        }
                    );
                    syspathJob->exec();
                }
            );
            brightnessMaxJob->exec();
        }
    );
    brightnessJob->exec();
}

