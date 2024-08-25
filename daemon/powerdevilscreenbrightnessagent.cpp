/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilscreenbrightnessagent.h"

#include <powerdevil_debug.h>
#include <screenbrightnessadaptor.h>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDebug>

#include <KScreen/Output>

using namespace Qt::Literals::StringLiterals;

namespace PowerDevil
{
ScreenBrightnessAgent::ScreenBrightnessAgent(QObject *parent, ScreenBrightnessController *controller)
    : QObject(parent)
    , m_controller(controller)
{
    new ScreenBrightnessAdaptor(this);
    QDBusConnection::sessionBus().registerObject(u"/org/kde/ScreenBrightness"_s, this);

    connect(m_controller, &ScreenBrightnessController::brightnessChanged, this, &ScreenBrightnessAgent::onBrightnessChanged);
    // TODO: handle max brightness change via new ScreenBrightnessController::brightnessRangeChanged signal?
    connect(m_controller, &ScreenBrightnessController::displayAdded, this, &ScreenBrightnessAgent::DisplayAdded);
    connect(m_controller, &ScreenBrightnessController::displayRemoved, this, &ScreenBrightnessAgent::DisplayRemoved);
}

void ScreenBrightnessAgent::onBrightnessChanged(const QString &displayId,
                                                const BrightnessLogic::BrightnessInfo &info,
                                                const QString &sourceClientName,
                                                const QString &sourceClientContext,
                                                ScreenBrightnessController::IndicatorHint hint)
{
    Q_EMIT BrightnessChanged(displayId, info.value - info.valueMin, sourceClientName, sourceClientContext);

    if (hint == ScreenBrightnessController::ShowIndicator) {
        // Try to match the controller's display to a KScreen display for optimized OSD presentation.
        const KScreen::OutputPtr matchedOutput = m_controller->tryMatchKScreenOutput(displayId);

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QLatin1String("screenBrightnessChanged"));
        msg << brightnessPercent(info.value - info.valueMin, info.valueMax - info.valueMin) << (matchedOutput ? matchedOutput->name() : displayId)
            << GetLabel(displayId) << static_cast<int>(m_controller->displayIds().indexOf(displayId))
            << (matchedOutput && matchedOutput->isPositionable() ? matchedOutput->geometry() : QRect());
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

void ScreenBrightnessAgent::onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &info)
{
    Q_EMIT BrightnessRangeChanged(displayId, info.valueMax - info.valueMin, info.value - info.valueMin);
}

QStringList ScreenBrightnessAgent::GetDisplayIds() const
{
    return m_controller->displayIds();
}

QString ScreenBrightnessAgent::GetLabel(const QString &displayId) const
{
    return m_controller->label(displayId);
}

bool ScreenBrightnessAgent::GetIsInternal(const QString &displayId) const
{
    return m_controller->isInternal(displayId);
}

int ScreenBrightnessAgent::GetBrightness(const QString &displayId) const
{
    return m_controller->brightness(displayId) - m_controller->minBrightness(displayId);
}

int ScreenBrightnessAgent::GetMaxBrightness(const QString &displayId) const
{
    return m_controller->maxBrightness(displayId) - m_controller->minBrightness(displayId);
}

void ScreenBrightnessAgent::AdjustBrightnessRatio(double delta, uint flags)
{
    AdjustBrightnessRatioWithContext(delta, flags, QString());
}

void ScreenBrightnessAgent::AdjustBrightnessRatioWithContext(double delta, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(AdjustBrightnessRatioFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    m_controller->adjustBrightnessRatio(delta, sourceClientName, sourceClientContext, hint);
}

void ScreenBrightnessAgent::AdjustBrightnessStep(uint stepAction, uint flags)
{
    AdjustBrightnessStepWithContext(stepAction, flags, QString());
}

void ScreenBrightnessAgent::AdjustBrightnessStepWithContext(uint stepAction, uint flags, const QString &sourceClientContext)
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

    m_controller->adjustBrightnessStep(adjustment, sourceClientName, sourceClientContext, hint);
}

void ScreenBrightnessAgent::SetBrightness(const QString &displayId, int value, uint flags)
{
    SetBrightnessWithContext(displayId, value, flags, QString());
}

void ScreenBrightnessAgent::SetBrightnessWithContext(const QString &displayId, int value, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(SetBrightnessFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    m_controller->setBrightness(displayId, value + m_controller->minBrightness(displayId), sourceClientName, sourceClientContext, hint);
}

int ScreenBrightnessAgent::brightnessPercent(double value, double max) const
{
    return max > 0 ? std::round(value / max * 100) : 0;
}
}

#include "moc_powerdevilscreenbrightnessagent.cpp"
