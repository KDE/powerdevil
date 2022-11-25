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

namespace PowerDevil {
namespace BundledActions {

class BrightnessControl : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(BrightnessControl)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.BrightnessControl")

public:
    explicit BrightnessControl(QObject *parent, const QVariantList &);
    ~BrightnessControl() override = default;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override;
    void triggerImpl(const QVariantMap& args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup& config) override;

    PerceivedBrightness perceivedBrightness() const;
    /**
     * @returns Perceived screen brightness
     * as a value on the scale [0, maxBrightness]
     */
    int brightness() const;
    /**
     * @returns The highest screen brightness value.
     */
    int brightnessMax() const;
    /**
     * @returns The highest screen brightness step.
     */
    int brightnessSteps() const;

public Q_SLOTS:
    // DBus export
    void increaseBrightness();
    void decreaseBrightness();
    /**
     * Sets the perceived brightness on
     * the scale [0, max brightness]
     */
    void setBrightness(int value);
    void setBrightnessSilent(int value);

private Q_SLOTS:
    void onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &brightnessInfo, BackendInterface::BrightnessControlType type);

Q_SIGNALS:
    /**
     * Emits perceived brightness of screen
     */
    void brightnessChanged(int perceivedValue);
    void brightnessMaxChanged(int valueMax);

private:
    /**
     * @returns [0, 100] value as a proportion of max brightness.
     * If max brightness is <= 0, returns 0.
     */
    PerceivedBrightness brightnessPercent(PerceivedBrightness value) const;

    int m_defaultValue = -1;
    QString m_currentProfile;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_BRIGHTNESSCONTROL_H
