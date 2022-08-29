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
    explicit KeyboardBrightnessControl(QObject* parent, const QVariantList &);
    ~KeyboardBrightnessControl() override = default;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override;
    /**
     * @arg Value perceived brightness int on [0, max brightness]
     */
    void triggerImpl(const QVariantMap& args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup& config) override;

    PerceivedBrightness perceivedKeyboardBrightness() const;
    /**
     * @returns Perceived brightness of the keyboard
     * on [0, brightnessMax]
     */
    int keyboardBrightness() const;
    int keyboardBrightnessMax() const;
    /**
     * @returns The highest brightness step allowed.
     * Does not return the highest value.
     */
    int keyboardBrightnessSteps() const;

public Q_SLOTS:
    void onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &brightnessInfo, BackendInterface::BrightnessControlType type);

    // DBus export
    void increaseKeyboardBrightness();
    void decreaseKeyboardBrightness();
    void toggleKeyboardBacklight();

    /**
     * Sets the keyboard brightness
     * @param value is a perceived brightness value
     * on [0, max brightness]
     */
    void setKeyboardBrightness(int value);
    void setKeyboardBrightnessSilent(int value);

Q_SIGNALS:
    /**
     * Emits the current perceived brightness value
     */
    void keyboardBrightnessChanged(int perceivedValue);
    void keyboardBrightnessMaxChanged(int valueMax);

private:
    /**
     * @returns Perceived keyboard brightness as a
     * [0, 100] int percent of max brightness.
     */
    PerceivedBrightness keyboardBrightnessPercent() const;

    PerceivedBrightness m_defaultValue = PerceivedBrightness(-1);
    PerceivedBrightness m_lastKeyboardBrightness = PerceivedBrightness(-1);
    QString m_lastProfile;
    QString m_currentProfile;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_KEYBOARDBRIGHTNESSCONTROL_H
