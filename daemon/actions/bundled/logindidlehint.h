/*
 *   SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>

namespace PowerDevil::BundledActions
{
class LogindIdleHintClient;

/**
 * Reports the idle state of the session to logind, as SetIdleHint, so that other programs can
 * follow it without a connection to the display server.
 *
 * The session is idle after SessionIdleTimeoutSec of the global settings without input, five minutes
 * by default, whatever the screen does. 0 never reports it idle.
 */
class LogindIdleHint : public PowerDevil::Action
{
    Q_OBJECT
public:
    explicit LogindIdleHint(QObject *parent);

    bool loadAction(const PowerDevil::ProfileSettings &profileSettings) override;

protected:
    void onIdleTimeout(std::chrono::milliseconds timeout) override;
    void onWakeupFromIdle() override;
    void onProfileUnload() override;

private:
    LogindIdleHintClient *m_client;
};
}
