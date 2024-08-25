/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>
#include <powerdevilbrightnesslogic.h>

namespace PowerDevil::BundledActions
{

class ScreenBrightnessControl : public PowerDevil::Action, protected QDBusContext
{
    Q_OBJECT

public:
    explicit ScreenBrightnessControl(QObject *parent);

protected:
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const PowerDevil::ProfileSettings &profileSettings) override;

private:
    void actOnBrightnessKey(BrightnessLogic::StepAdjustmentAction action);
};

}
