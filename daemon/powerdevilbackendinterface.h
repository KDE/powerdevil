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

#pragma once

#include <QObject>
#include <QHash>

#include "powerdevilbrightnesslogic.h"

#include "powerdevilcore_export.h"

class KJob;

namespace PowerDevil {

class POWERDEVILCORE_EXPORT BackendInterface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BackendInterface)

public:
    explicit BackendInterface(QObject* parent = nullptr);
    ~BackendInterface() override;

    /**
     * This enum type defines the different states of the AC adapter.
     *
     * - UnknownAcAdapterState: The AC adapter has an unknown state
     * - Plugged: The AC adapter is plugged
     * - Unplugged: The AC adapter is unplugged
     */
    enum AcAdapterState{ UnknownAcAdapterState, Plugged, Unplugged };
    Q_ENUM(AcAdapterState)

    /**
     * This enum type defines the types of system button events.
     *
     * - UnknownButtonType: An unknown button
     * - PowerDown: A power down pressed event, generally used to turn on or off the system. KWin emits on long power button presses.
     * - PowerButton: A power button pressed event, generally used to turn on or off the system
     * - SleepButton: A sleep button pressed event, generally used to make the system asleep
     * - LidOpen: A laptop lid open event
     * - LidClose: A laptop lid close event
     */
    enum ButtonType{ UnknownButtonType, PowerButton, PowerDownButton, SleepButton, LidOpen, LidClose, HibernateButton };
    Q_ENUM(ButtonType)

    /**
     * This enum type defines the different suspend methods.
     *
     * - UnknownSuspendMethod: The name says it all
     * - Standby: Processes are stopped, some hardware is deactivated (ACPI S1)
     * - ToRam: Most devices are deactivated, only RAM is powered (ACPI S3)
     * - ToDisk: State of the machine is saved to disk, and it's powered down (ACPI S4)
     * - SuspendThenHibernate: Same as ToRam, but after a delay it switches to ToDisk
     */
    enum SuspendMethod{ UnknownSuspendMethod = 0, Standby = 1, ToRam = 2, ToDisk = 4, HybridSuspend = 8, SuspendThenHibernate = 16 };
    Q_ENUM(SuspendMethod)

    /**
     * This type stores an OR combination of SuspendMethod values.
     */
    Q_DECLARE_FLAGS(SuspendMethods, SuspendMethod)

    /**
     * This enum defines the different types of brightness controls.
     *
     * - UnknownBrightnessControl: Unknown
     * - Screen: Brightness control for a monitor or laptop panel
     * - Keyboard: Brightness control for a keyboard backlight
     */
    enum BrightnessControlType{ UnknownBrightnessControl = 0, Screen = 1, Keyboard = 2 };
    Q_ENUM(BrightnessControlType)

    typedef QHash<QString, BrightnessControlType> BrightnessControlsList;

    /**
     * This enum defines capabilities of the backend
     *
     * - SignalResumeFromSuspend: The backend is able to stream the @c resumeFromSuspend signal accurately
     */
    enum Capability { NoCapabilities = 0, SignalResumeFromSuspend = 1 };
    Q_ENUM(Capability)

    Q_DECLARE_FLAGS(Capabilities, Capability)

    /**
     * Initializes the backend. This function @b MUST be called before the backend is usable. Using
     * any method in BackendInterface without initializing it might lead to undefined behavior. The signal
     * @c backendReady or @c backendError will be streamed upon completion.
     *
     * @note Backend implementations @b MUST reimplement this function
     */
    virtual void init() = 0;

    /**
     * @returns the capabilities of the backend
     * @see PowerDevil::BackendInterface::Capability
     */
    Capabilities capabilities() const;

    /**
     * Retrieves the current estimated remaining time of the system batteries
     *
     * @return the current global estimated remaining time in milliseconds
     */
    qulonglong batteryRemainingTime() const;

    /**
     * Retrieves the current estimated remaining time of the system batteries,
     * with exponential moving average filter applied to the history records.
     *
     * @return the current global estimated remaining time in milliseconds
     */
    qulonglong smoothedBatteryRemainingTime() const;

    /**
     * Retrieves the current state of the system AC adapter.
     *
     * @return the current AC adapter state
     * @see PowerDevil::BackendInterface::AcAdapterState
     */
    AcAdapterState acAdapterState() const;

    /**
     * Retrieves the set of suspend methods supported by the system.
     *
     * @return the suspend methods supported by this system
     * @see PowerDevil::BackendInterface::SuspendMethod
     * @see PowerDevil::BackendInterface::SuspendMethods
     */
    SuspendMethods supportedSuspendMethods() const;

    /**
     * Requests a suspend of the system.
     *
     * @param method the suspend method to use
     * @return the job handling the operation
     */
    virtual KJob *suspend(SuspendMethod method) = 0;

    /**
     * Checks if brightness controls are enabled on this system.
     *
     * @return a list of the devices available to control
     */
    BrightnessControlsList brightnessControlsAvailable() const;

    /**
     * Gets the screen brightness value.
     *
     * @return the brightness of the screen, as an integer from 0 to brightnessValueMax
     */
    virtual int screenBrightness() const = 0;

    /**
     * Gets the maximum device brightness value.
     *
     * @param device the name of the device that you would like to control
     * @return the maximum brightness of the device
     */
    virtual int screenBrightnessMax() const = 0;

    int screenBrightnessSteps() const;

    virtual void setScreenBrightness(int value) = 0;

    virtual int screenBrightnessKeyPressed(BrightnessLogic::BrightnessKeyType type) = 0;

    virtual int keyboardBrightness() const = 0;

    virtual int keyboardBrightnessMax() const = 0;

    int keyboardBrightnessSteps() const;

    virtual void setKeyboardBrightness(int value) = 0;

    virtual int keyboardBrightnessKeyPressed(BrightnessLogic::BrightnessKeyType type) = 0;

    /**
     * @returns whether the lid is closed or not.
     */
    bool isLidClosed() const;
    /**
     * @returns whether the a lid is present
     */
    bool isLidPresent() const;

    void setLidPresent(bool present);

Q_SIGNALS:
    /**
     * This signal is emitted when the AC adapter is plugged or unplugged.
     *
     * @param newState the new state of the AC adapter, it's one of the
     * type @see PowerDevil::BackendInterface::AcAdapterState
     */
    void acAdapterStateChanged(PowerDevil::BackendInterface::AcAdapterState newState);

    /**
     * This signal is emitted when a button has been pressed.
     *
     * @param buttonType the pressed button type, it's one of the
     * type @see PowerDevil::BackendInterface::ButtonType
     */
    void buttonPressed(PowerDevil::BackendInterface::ButtonType buttonType);

    void screenBrightnessChanged(const BrightnessLogic::BrightnessInfo &brightnessInfo);

    void keyboardBrightnessChanged(const BrightnessLogic::BrightnessInfo &brightnessInfo);

    /**
     * This signal is emitted when the estimated battery remaining time changes.
     *
     * @param time the new remaining time
     */
    void batteryRemainingTimeChanged(qulonglong time);

    /**
     * This signal is emitted when the estimated battery remaining time changes.
     *
     * @param time the new remaining time
     */
    void smoothedBatteryRemainingTimeChanged(qulonglong time);

    /**
     * This signal is emitted when the backend is ready to be used
     *
     * @see init
     */
    void backendReady();

    /**
     * This signal is emitted if the backend could not be initialized
     *
     * @param error Details about the error occurred
     * @see init
     */
    void backendError(const QString &error);

    /**
     * This signal is emitted when the PC is resuming from suspension
     */
    void resumeFromSuspend();

    /**
     * This signal is emitted when the PC is about to suspend
     */
    void aboutToSuspend();

    /**
     * This signal is emitted when the laptop lid is closed or opened
     *
     * @param closed Whether the lid is now closed or not
     */
    void lidClosedChanged(bool closed);

protected:
    void setCapabilities(Capabilities capabilities);

    void onScreenBrightnessChanged(int value, int valueMax);
    void onKeyboardBrightnessChanged(int value, int valueMax);
    void setBatteryEnergy(double energy);
    void setBatteryEnergyFull(double energy);
    void setBatteryRate(double rate, qulonglong timestamp);
    void setButtonPressed(PowerDevil::BackendInterface::ButtonType type);
    void setAcAdapterState(PowerDevil::BackendInterface::AcAdapterState state);

    void setBackendIsReady(const BrightnessControlsList &availableBrightnessControls, SuspendMethods supportedSuspendMethods);
    void setBackendHasError(const QString &errorDetails);

    // Steps logic
    int calculateNextStep(int value, int valueMax, BrightnessControlType controlType, BrightnessLogic::BrightnessKeyType keyType);

private:
    class Private;
    Private * const d;

    friend class Core;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(PowerDevil::BackendInterface::Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(PowerDevil::BackendInterface::SuspendMethods)
