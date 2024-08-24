/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnesscontrol.h"

#include <powerdevilcore.h>

#include <PowerDevilProfileSettings.h>
#include <screenbrightnesscontroladaptor.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KScreen/Output>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::ScreenBrightnessControl, "powerdevilscreenbrightnesscontrolaction.json")

using namespace Qt::Literals::StringLiterals;

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

    KActionCollection *actionCollection = new KActionCollection(this);
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));

    QAction *globalAction = actionCollection->addAction("Increase Screen Brightness"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessUp);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::Increase);
    });

    globalAction = actionCollection->addAction("Increase Screen Brightness Small"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness by 1%"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::ShiftModifier | Qt::Key_MonBrightnessUp);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::IncreaseSmall);
    });

    globalAction = actionCollection->addAction("Decrease Screen Brightness"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessDown);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::Decrease);
    });

    globalAction = actionCollection->addAction("Decrease Screen Brightness Small"_L1);
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness by 1%"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::ShiftModifier | Qt::Key_MonBrightnessDown);
    connect(globalAction, &QAction::triggered, this, [this] {
        actOnBrightnessKey(BrightnessLogic::DecreaseSmall);
    });
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

    if (hint == ScreenBrightnessController::ShowIndicator) {
        // Try to match the controller's display to a KScreen display for optimized OSD presentation.
        const KScreen::OutputPtr matchedOutput = core()->screenBrightnessController()->tryMatchKScreenOutput(displayId);

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QLatin1String("screenBrightnessChanged"));
        msg << brightnessPercent(info.value - info.valueMin, info.valueMax - info.valueMin) << (matchedOutput ? matchedOutput->name() : displayId)
            << GetLabel(displayId) << static_cast<int>(core()->screenBrightnessController()->displayIds().indexOf(displayId))
            << (matchedOutput && matchedOutput->isPositionable() ? matchedOutput->geometry() : QRect());
        QDBusConnection::sessionBus().asyncCall(msg);
    }
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

void ScreenBrightnessControl::actOnBrightnessKey(BrightnessLogic::StepAdjustmentAction action)
{
    core()->screenBrightnessController()->adjustBrightnessStep(action, u"(internal)"_s, u"brightness_key"_s, ScreenBrightnessController::ShowIndicator);
}

int ScreenBrightnessControl::brightnessPercent(double value, double max) const
{
    return max > 0 ? qRound(value / max * 100) : 0;
}
}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
