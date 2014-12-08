/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/


#ifndef POWERDEVIL_BUNDLEDACTIONS_BRIGHTNESSCONTROL_H
#define POWERDEVIL_BUNDLEDACTIONS_BRIGHTNESSCONTROL_H

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

class BrightnessOSDWidget;

namespace PowerDevil {
namespace BundledActions {

class BrightnessControl : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(BrightnessControl)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.BrightnessControl")

public:
    explicit BrightnessControl(QObject* parent);
    virtual ~BrightnessControl();

protected:
    virtual void onProfileUnload();
    virtual void onWakeupFromIdle();
    virtual void onIdleTimeout(int msec);
    virtual void onProfileLoad();
    virtual void triggerImpl(const QVariantMap& args);
    virtual bool isSupported();

public:
    virtual bool loadAction(const KConfigGroup& config);

public Q_SLOTS:
    void showBrightnessOSD(int brightness);

    // DBus export
    void increaseBrightness();
    void decreaseBrightness();
    int brightness() const;
    void setBrightness(int percent);

    int brightnessValue() const;
    int brightnessValueMax() const;
    void setBrightnessValue(int value);

    int brightnessStep() const;
    int brightnessStepMax() const;
    void setBrightnessStep(int step);

private Q_SLOTS:
    void onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &brightnessInfo, BackendInterface::BrightnessControlType type);

Q_SIGNALS:
    void brightnessChanged(int percent);
    void brightnessValueChanged(int value);
    void brightnessValueMaxChanged(int valueMax);
    void brightnessStepChanged(int step);

private:
    int m_defaultValue;
    BrightnessOSDWidget *m_brightnessOSD;
    QString m_lastProfile;
    QString m_currentProfile;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_BRIGHTNESSCONTROL_H
