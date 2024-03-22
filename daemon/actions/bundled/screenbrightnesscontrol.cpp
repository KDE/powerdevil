/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnesscontrol.h"

#include <PowerDevilProfileSettings.h>
#include <brightnessosdwidget.h>
#include <powerdevilcore.h>
#include <screenbrightnesscontroladaptor.h>

#include <QAction>
#include <QDebug>

#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::ScreenBrightnessControl, "powerdevilscreenbrightnesscontrolaction.json")

namespace PowerDevil::BundledActions
{
ScreenBrightnessControl::ScreenBrightnessControl(QObject *parent)
    : Action(parent)
{
    // DBus
    new ScreenBrightnessControlAdaptor(this);

    connect(core()->screenBrightnessController(),
            &ScreenBrightnessController::brightnessChanged,
            this,
            &PowerDevil::BundledActions::ScreenBrightnessControl::onBrightnessChanged);
    // TODO: handle max brightness change via new ScreenBrightnessController::brightnessRangeChanged signal?
    connect(core()->screenBrightnessController(),
            &ScreenBrightnessController::displayAdded,
            this,
            &PowerDevil::BundledActions::ScreenBrightnessControl::DisplayAdded);
    connect(core()->screenBrightnessController(),
            &ScreenBrightnessController::displayRemoved,
            this,
            &PowerDevil::BundledActions::ScreenBrightnessControl::DisplayRemoved);
}

void ScreenBrightnessControl::onProfileLoad(const QString &previousProfile, const QString &newProfile)
{
    // For the time being, let the (legacy) BrightnessControl action set a single brightness value
    // on profile load. Once we're able to store brightness values per display, we can implement
    // this here to replace BrightnessControl's implementation.
    Q_UNUSED(previousProfile)
    Q_UNUSED(newProfile)
}

void ScreenBrightnessControl::triggerImpl(const QVariantMap & /*args*/)
{
}

bool ScreenBrightnessControl::isSupported()
{
    // The set of displays might be empty, but the D-Bus interface will always be exposed.
    return true;
}

bool ScreenBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)

    return false;
}

void ScreenBrightnessControl::onBrightnessChanged(const QString &displayId,
                                                  const BrightnessLogic::BrightnessInfo &info,
                                                  const QString &sourceClientName,
                                                  const QString &sourceClientContext,
                                                  ScreenBrightnessController::IndicatorHint hint)
{
    Q_EMIT BrightnessChanged(displayId, info.value - info.valueMin, sourceClientName, sourceClientContext);

    // OSD here is currently disabled to avoid duplicating BrightnessControl's OSD.
    // TODO: Implement per-display OSDs here and remove code to show the OSD from BrightnessControl.
    Q_UNUSED(hint)
    // if (hint == ScreenBrightnessController::ShowIndicator) {
    //     BrightnessOSDWidget::show(brightnessPercent(info.value - info.valueMin));
    // }
}

void ScreenBrightnessControl::onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &info)
{
    Q_EMIT BrightnessRangeChanged(displayId, info.valueMax - info.valueMin, info.value - info.valueMin);
}

QStringList ScreenBrightnessControl::GetDisplayIds() const
{
    return core()->screenBrightnessController()->displayIds();
}

QString ScreenBrightnessControl::GetLabel(const QString &displayId) const
{
    return core()->screenBrightnessController()->label(displayId);
}

bool ScreenBrightnessControl::GetIsInternal(const QString &displayId) const
{
    return core()->screenBrightnessController()->isInternal(displayId);
}

int ScreenBrightnessControl::GetBrightness(const QString &displayId) const
{
    return core()->screenBrightnessController()->brightness(displayId) - core()->screenBrightnessController()->minBrightness(displayId);
}

int ScreenBrightnessControl::GetMaxBrightness(const QString &displayId) const
{
    return core()->screenBrightnessController()->maxBrightness(displayId) - core()->screenBrightnessController()->minBrightness(displayId);
}

void ScreenBrightnessControl::AdjustBrightnessRatio(double delta, uint flags)
{
    AdjustBrightnessRatioWithContext(delta, flags, QString());
}

void ScreenBrightnessControl::AdjustBrightnessRatioWithContext(double delta, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(AdjustBrightnessRatioFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    core()->screenBrightnessController()->adjustBrightnessRatio(delta, sourceClientName, sourceClientContext, hint);
}

void ScreenBrightnessControl::AdjustBrightnessStep(uint stepAction, uint flags)
{
    AdjustBrightnessStepWithContext(stepAction, flags, QString());
}

void ScreenBrightnessControl::AdjustBrightnessStepWithContext(uint stepAction, uint flags, const QString &sourceClientContext)
{
    BrightnessLogic::StepAdjustmentAction adjustment;
    switch (static_cast<AdjustBrightnessStepAction>(stepAction)) {
    case AdjustBrightnessStepAction::Increase:
        adjustment = BrightnessLogic::Increase;
        break;
    case AdjustBrightnessStepAction::Decrease:
        adjustment = BrightnessLogic::Decrease;
        break;
    case AdjustBrightnessStepAction::IncreaseSmall:
        adjustment = BrightnessLogic::IncreaseSmall;
        break;
    case AdjustBrightnessStepAction::DecreaseSmall:
        adjustment = BrightnessLogic::DecreaseSmall;
        break;
    default:
        qCDebug(POWERDEVIL) << "Adjust brightness step: Unknown step action:" << stepAction;
        return;
    }

    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(AdjustBrightnessStepFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    core()->screenBrightnessController()->adjustBrightnessStep(adjustment, sourceClientName, sourceClientContext, hint);
}

void ScreenBrightnessControl::SetBrightness(const QString &displayId, int value, uint flags)
{
    SetBrightnessWithContext(displayId, value, flags, QString());
}

void ScreenBrightnessControl::SetBrightnessWithContext(const QString &displayId, int value, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(SetBrightnessFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    core()->screenBrightnessController()->setBrightness(displayId,
                                                        value + core()->screenBrightnessController()->minBrightness(displayId),
                                                        sourceClientName,
                                                        sourceClientContext,
                                                        hint);
}

int ScreenBrightnessControl::brightnessPercent(const QString &displayId, double value) const
{
    const double max = GetMaxBrightness(displayId);
    return max > 0 ? qRound(value / max * 100) : 0;
}

}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
