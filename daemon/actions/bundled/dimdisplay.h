/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>

namespace PowerDevil::BundledActions
{
class DimDisplay : public PowerDevil::Action
{
    Q_OBJECT
public:
    explicit DimDisplay(QObject *parent);

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup &config) override;

private:
    void setBrightnessHelper(int screen, int keyboard, bool force = false);

    int m_dimOnIdleTime = 0;

    int m_oldScreenBrightness = 0;
    int m_oldKeyboardBrightness = 0;

    bool m_dimmed = false;
};

}
