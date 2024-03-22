/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>
#include <powerdevilbrightnesslogic.h>

#include <QDBusContext>

namespace PowerDevil::BundledActions
{

enum class SetBrightnessFlags {
    SuppressIndicator = 0x1, /// Don't show brightness change indicators such as OSD, notification, etc.
};

class ScreenBrightnessControl : public PowerDevil::Action, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl")

public:
    explicit ScreenBrightnessControl(QObject *parent);

protected:
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const PowerDevil::ProfileSettings &profileSettings) override;

    // D-Bus export
    QStringList displayIds() const;
    QString label(const QString &displayId) const;
    int brightness(const QString &displayId) const;
    int maxBrightness(const QString &displayId) const;
    int minBrightness(const QString &displayId) const;
    int knownSafeMinBrightness(const QString &displayId) const;
    int preferredNumberOfBrightnessSteps(const QString &displayId) const;

public Q_SLOTS:
    // D-Bus export
    void setBrightness(const QString &displayId, int value, const QString &sourceClientContext, uint flags = 0x0);

private Q_SLOTS:
    void onDisplayAdded(const QString &displayId);
    void onDisplayRemoved(const QString &displayId);
    void onBrightnessChanged(const QString &displayId,
                             const BrightnessLogic::BrightnessInfo &brightnessInfo,
                             const QString &sourceClientName,
                             const QString &sourceClientContext);
    void onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &brightnessInfo);

Q_SIGNALS:
    // D-Bus export
    void DisplayAdded(const QString &displayId);
    void DisplayRemoved(const QString &displayId);
    void BrightnessChanged(const QString &displayId, int brightness, const QString &sourceClientName, const QString &sourceClientContext);
    void BrightnessRangeChanged(const QString &displayId, int minBrightness, int maxBrightness, int brightness);

private:
    int brightnessPercent(const QString &displayId, float value) const;
};

}
