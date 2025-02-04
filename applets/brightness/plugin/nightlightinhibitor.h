/*
 * SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 * SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/**
 * The Inhibitor class provides a convenient way to temporarily disable Night Light.
 */
class NightLightInhibitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    /**
     * This property holds whether Night Light is currently inhibited, inhibiting or has a pending inhibit.
     */
    Q_PROPERTY(bool inhibited READ isInhibited NOTIFY inhibitedChanged)

public:
    static NightLightInhibitor *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    /**
     * This enum type is used to specify the state of the inhibitor.
     */
    enum State {
        Inhibiting, ///< Night Light is being inhibited.
        Inhibited, ///< Night Light is inhibited.
        Uninhibiting, ///< Night Light is being uninhibited.
        Uninhibited, ///< Night Light is uninhibited.
    };
    Q_ENUM(State)

    /**
     * Returns true if Night Light is currently inhibited, inhibiting or has a pending inhibit, and false if it is uninhibit or uninhibiting.
     */
    bool isInhibited() const;

    /**
     *  Attemmpts to temporarily disable Night Light if currently uninhibited or uninhibiting, and uto re-enable it if currently inhibited or inhibiting.
     */
    Q_INVOKABLE void toggleInhibition();

Q_SIGNALS:
    /**
     * This signal is emitted when the state of the inhibitor has changed.
     */
    void inhibitedChanged();

private:
    explicit NightLightInhibitor(QObject *parent = nullptr);
    ~NightLightInhibitor() override;
    class Private;

    uint m_cookie = 0;
    State m_state = Uninhibited;
    bool m_pendingUninhibit = false;

    /**
     * Attempts to temporarily disable Night Light.
     *
     * After calling this method, the inhibitor will enter the Inhibiting state.
     * Eventually, the inhibitor will enter the Inhibited state when the inhibition
     * request has been processed successfully by the Night Light manager.
     *
     * This method does nothing if the inhibitor is in the Inhibited state.
     */
    void inhibit();

    /**
     * Attempts to undo the previous call to inhibit() method.
     *
     * After calling this method, the inhibitor will enter the Uninhibiting state.
     * Eventually, the inhibitor will enter the Uninhibited state when the uninhibition
     * request has been processed by the Night Light manager.
     *
     * This method does nothing if the inhibitor is in the Uninhibited state.
     */
    void uninhibit();
};
