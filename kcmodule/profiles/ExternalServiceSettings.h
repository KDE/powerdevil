/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

class QWindow;

namespace PowerDevil
{

class ExternalServiceSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int chargeStartThreshold READ chargeStartThreshold WRITE setChargeStartThreshold NOTIFY chargeStartThresholdChanged)
    Q_PROPERTY(int chargeStopThreshold READ chargeStopThreshold WRITE setChargeStopThreshold NOTIFY chargeStopThresholdChanged)

public:
    explicit ExternalServiceSettings(QObject *parent);

    bool isSaveNeeded() const;

    int chargeStartThreshold() const;
    int chargeStopThreshold() const;

    bool isChargeStartThresholdSupported() const;
    bool isChargeStopThresholdSupported() const;
    bool chargeStopThresholdMightNeedReconnect() const;

public Q_SLOTS:
    void load(QWindow *parentWindowForKAuth = nullptr);
    void save(QWindow *parentWindowForKAuth = nullptr);

    void setChargeStartThreshold(int);
    void setChargeStopThreshold(int);

Q_SIGNALS:
    void settingsChanged();

    // settings, which in addition to their own signal also trigger settingsChanged()
    void chargeStartThresholdChanged();
    void chargeStopThresholdChanged();

    // not settings per se, so these don't trigger settingsChanged()
    void isChargeStartThresholdSupportedChanged();
    void isChargeStopThresholdSupportedChanged();
    void chargeStopThresholdMightNeedReconnectChanged();

private:
    void setSavedChargeStartThreshold(int);
    void setSavedChargeStopThreshold(int);
    void setChargeStopThresholdMightNeedReconnect(bool);

    int m_chargeStartThreshold;
    int m_chargeStopThreshold;
    int m_savedChargeStartThreshold;
    int m_savedChargeStopThreshold;
    bool m_chargeStopThresholdMightNeedReconnect;
};

} // namespace PowerDevil
