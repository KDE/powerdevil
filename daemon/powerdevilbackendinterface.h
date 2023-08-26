/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QObject>

#include "powerdevilbrightnesslogic.h"
#include "powerdevilkeyboardbrightnesslogic.h"
#include "powerdevilscreenbrightnesslogic.h"

#include "powerdevilcore_export.h"

class KJob;

namespace PowerDevil
{
class POWERDEVILCORE_EXPORT BackendInterface : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BackendInterface)

public:
    explicit BackendInterface(QObject *parent = nullptr);
    ~BackendInterface() override;

    /**
     * This enum type defines the different states of the AC adapter.
     *
     * - UnknownAcAdapterState: The AC adapter has an unknown state
     * - Plugged: The AC adapter is plugged
     * - Unplugged: The AC adapter is unplugged
     */
    enum AcAdapterState { UnknownAcAdapterState, Plugged, Unplugged };
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
    enum ButtonType { UnknownButtonType, PowerButton, PowerDownButton, SleepButton, LidOpen, LidClose, HibernateButton };
    Q_ENUM(ButtonType)

    /**
     * This enum defines the different types of brightness controls.
     *
     * - UnknownBrightnessControl: Unknown
     * - Screen: Brightness control for a monitor or laptop panel
     * - Keyboard: Brightness control for a keyboard backlight
     */
    enum BrightnessControlType { UnknownBrightnessControl = 0, Screen = 1, Keyboard = 2 };
    Q_ENUM(BrightnessControlType)

    /**
     * Initializes the backend. This function @b MUST be called before the backend is usable. Using
     * any method in BackendInterface without initializing it might lead to undefined behavior. The signal
     * @c backendReady or @c backendError will be streamed upon completion.
     *
     * @note Backend implementations @b MUST reimplement this function
     */
    virtual void init() = 0;

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

    int screenBrightnessSteps();

    virtual void setScreenBrightness(int value) = 0;

    virtual int screenBrightnessKeyPressed(BrightnessLogic::BrightnessKeyType type) = 0;

    virtual bool screenBrightnessAvailable() const = 0;

    virtual int keyboardBrightness() const = 0;

    virtual int keyboardBrightnessMax() const = 0;

    int keyboardBrightnessSteps();

    virtual void setKeyboardBrightness(int value) = 0;

    virtual int keyboardBrightnessKeyPressed(BrightnessLogic::BrightnessKeyType type) = 0;

    virtual bool keyboardBrightnessAvailable() const = 0;

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
     * This signal is emitted when the laptop lid is closed or opened
     *
     * @param closed Whether the lid is now closed or not
     */
    void lidClosedChanged(bool closed);

protected:
    void onScreenBrightnessChanged(int value, int valueMax);
    void onKeyboardBrightnessChanged(int value, int valueMax, bool notify = false);
    void setBatteryEnergy(double energy);
    void setBatteryEnergyFull(double energy);
    void setBatteryRate(double rate, qulonglong timestamp);
    void setButtonPressed(PowerDevil::BackendInterface::ButtonType type);
    void setAcAdapterState(PowerDevil::BackendInterface::AcAdapterState state);

    void setBackendIsReady();
    void setBackendHasError(const QString &errorDetails);

    // Steps logic
    int calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType);
    int calculateNextKeyboardBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType);

private:
    AcAdapterState m_acAdapterState = UnknownAcAdapterState;

    qulonglong m_batteryRemainingTime = 0;
    qulonglong m_smoothedBatteryRemainingTime = 0;
    qulonglong m_lastRateTimestamp = 0;
    double m_batteryEnergyFull = 0;
    double m_batteryEnergy = 0;
    double m_smoothedBatteryDischargeRate = 0;

    ScreenBrightnessLogic m_screenBrightnessLogic;
    KeyboardBrightnessLogic m_keyboardBrightnessLogic;
    QString m_errorString;
    bool m_isLidClosed = false;
    bool m_isLidPresent = false;

    friend class Core;

protected:
    int m_keyboardBrightnessBeforeTogglingOff;
};

}
