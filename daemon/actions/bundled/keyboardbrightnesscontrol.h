/***************************************************************************
 *   Copyright (C) 2012 by Michael Zanetti <mzanetti@kde.org>              *
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


#ifndef POWERDEVIL_BUNDLEDACTIONS_KEYBOARDBRIGHTNESSCONTROL_H
#define POWERDEVIL_BUNDLEDACTIONS_KEYBOARDBRIGHTNESSCONTROL_H

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

namespace PowerDevil {
namespace BundledActions {

class KeyboardBrightnessControl : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(KeyboardBrightnessControl)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl")

public:
    explicit KeyboardBrightnessControl(QObject* parent);
    ~KeyboardBrightnessControl() override = default;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override;
    void triggerImpl(const QVariantMap& args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup& config) override;

public Q_SLOTS:
    void onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &brightnessInfo, BackendInterface::BrightnessControlType type);

    // DBus export
    void increaseKeyboardBrightness();
    void decreaseKeyboardBrightness();
    void toggleKeyboardBacklight();

    int keyboardBrightness() const;
    int keyboardBrightnessMax() const;
    void setKeyboardBrightness(int value);
    void setKeyboardBrightnessSilent(int value);

    int keyboardBrightnessSteps() const;

Q_SIGNALS:
    void keyboardBrightnessChanged(int value);
    void keyboardBrightnessMaxChanged(int valueMax);

private:
    int keyboardBrightnessPercent() const;

    int m_defaultValue = -1;
    QString m_lastProfile;
    QString m_currentProfile;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_KEYBOARDBRIGHTNESSCONTROL_H
