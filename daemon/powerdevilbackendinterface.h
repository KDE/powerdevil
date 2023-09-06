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
     * This enum type defines the types of system button events.
     *
     * - UnknownButtonType: An unknown button
     * - PowerDown: A power down pressed event, generally used to turn on or off the system. KWin emits on long power button presses.
     * - PowerButton: A power button pressed event, generally used to turn on or off the system
     * - SleepButton: A sleep button pressed event, generally used to make the system asleep
     */
    enum ButtonType { UnknownButtonType, PowerButton, PowerDownButton, SleepButton, HibernateButton };
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
     * @c backendReady will be streamed upon completion.
     *
     * @note Backend implementations @b MUST reimplement this function
     */
    virtual void init() = 0;

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

Q_SIGNALS:
    void screenBrightnessChanged(const BrightnessLogic::BrightnessInfo &brightnessInfo);

    void keyboardBrightnessChanged(const BrightnessLogic::BrightnessInfo &brightnessInfo);

    /**
     * This signal is emitted when the backend is ready to be used
     *
     * @see init
     */
    void backendReady();

protected:
    void onScreenBrightnessChanged(int value, int valueMax);
    void onKeyboardBrightnessChanged(int value, int valueMax, bool notify = false);

    void setBackendIsReady();

    // Steps logic
    int calculateNextScreenBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType);
    int calculateNextKeyboardBrightnessStep(int value, int valueMax, BrightnessLogic::BrightnessKeyType keyType);

private:
    ScreenBrightnessLogic m_screenBrightnessLogic;
    KeyboardBrightnessLogic m_keyboardBrightnessLogic;

    friend class Core;

protected:
    int m_keyboardBrightnessBeforeTogglingOff;
};

}
