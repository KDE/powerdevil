/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "brightnesscontrol.h"

#include "brightnessosdwidget.h"

#include "brightnesscontroladaptor.h"

#include <powerdevilcore.h>
#include <powerdevil_debug.h>

#include <QDesktopWidget>
#include <QAction>
#include <QDebug>

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KGlobalAccel>
namespace PowerDevil {
namespace BundledActions {

BrightnessControl::BrightnessControl(QObject* parent)
    : Action(parent)
{
    // DBus
    new BrightnessControlAdaptor(this);

    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    connect(core()->backend(), &PowerDevil::BackendInterface::brightnessChanged, this, &PowerDevil::BundledActions::BrightnessControl::onBrightnessChangedFromBackend);

    KActionCollection* actionCollection = new KActionCollection( this );
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));

    QAction* globalAction = actionCollection->addAction(QLatin1String("Increase Screen Brightness"));
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessUp);
    connect(globalAction, &QAction::triggered, this, &BrightnessControl::increaseBrightness);

    globalAction = actionCollection->addAction(QLatin1String("Decrease Screen Brightness"));
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness"));
    KGlobalAccel::setGlobalShortcut(globalAction, Qt::Key_MonBrightnessDown);
    connect(globalAction, &QAction::triggered, this, &BrightnessControl::decreaseBrightness);
}

void BrightnessControl::onProfileUnload()
{
    //
}

void BrightnessControl::onWakeupFromIdle()
{
    //
}

void BrightnessControl::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void BrightnessControl::onProfileLoad()
{
    const int absoluteBrightnessValue = qRound(m_defaultValue / 100.0 * brightnessMax());

    // The core switches its profile after all actions have been updated, so here core()->currentProfile() is still the old one
    const auto previousProfile = core()->currentProfile();
    qCDebug(POWERDEVIL) << "Profiles: " << m_currentProfile << previousProfile;

    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((m_currentProfile == QLatin1String("Battery") && previousProfile == QLatin1String("AC")) ||
         (m_currentProfile == QLatin1String("LowBattery") && (previousProfile == QLatin1String("AC") || previousProfile == QLatin1String("Battery")))) &&
        absoluteBrightnessValue > brightness()) {

        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing brightness, the current one is lower and the profile is more conservative";
    } else if (absoluteBrightnessValue >= 0) {
        QVariantMap args{
            {QStringLiteral("Value"), QVariant::fromValue(absoluteBrightnessValue)}
        };

        // plugging in/out the AC is always explicit
        if ((previousProfile == QLatin1String("AC") && m_currentProfile != QLatin1String("AC")) ||
            (previousProfile != QLatin1String("AC") && m_currentProfile == QLatin1String("AC"))) {
            args["Explicit"] = true;
            args["Silent"] = true; // but we still don't want to show the OSD then
        }

        trigger(args);
    }
}

void BrightnessControl::triggerImpl(const QVariantMap &args)
{
    const int value = args.value(QStringLiteral("Value")).toInt();

    backend()->setBrightness(value);
    if (args.value(QStringLiteral("Explicit")).toBool() && !args.value(QStringLiteral("Silent")).toBool()) {
        BrightnessOSDWidget::show(brightnessPercent(value));
    }
}

bool BrightnessControl::isSupported()
{
    BackendInterface::BrightnessControlsList controls = backend()->brightnessControlsAvailable();
    if (controls.key(BackendInterface::Screen).isEmpty()) {
        return false;
    }

    return true;
}

bool BrightnessControl::loadAction(const KConfigGroup& config)
{
    // Handle profile changes
    m_currentProfile = config.parent().name();

    if (config.hasKey("value")) {
        m_defaultValue = config.readEntry<int>("value", 50);
    }

    return true;
}

void BrightnessControl::onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &info, BackendInterface::BrightnessControlType type)
{
    if (type == BackendInterface::Screen) {
        Q_EMIT brightnessChanged(info.value);
        Q_EMIT brightnessMaxChanged(info.valueMax);
    }
}

int BrightnessControl::brightness() const
{
    return backend()->brightness();
}

int BrightnessControl::brightnessMax() const
{
    return backend()->brightnessMax();
}

void BrightnessControl::setBrightness(int value)
{
    trigger({
        {QStringLiteral("Value"), QVariant::fromValue(value)},
        {QStringLiteral("Explicit"), true}
    });
}

void BrightnessControl::setBrightnessSilent(int value)
{
    trigger({
        {QStringLiteral("Value"), QVariant::fromValue(value)},
        {QStringLiteral("Explicit"), true},
        {QStringLiteral("Silent"), true}
    });
}

void BrightnessControl::increaseBrightness()
{
    const int newBrightness = backend()->brightnessKeyPressed(BrightnessLogic::Increase);
    if (newBrightness > -1) {
        BrightnessOSDWidget::show(brightnessPercent(newBrightness));
    }
}

void BrightnessControl::decreaseBrightness()
{
    const int newBrightness = backend()->brightnessKeyPressed(BrightnessLogic::Decrease);
    if (newBrightness > -1) {
        BrightnessOSDWidget::show(brightnessPercent(newBrightness));
    }
}

int BrightnessControl::brightnessSteps() const
{
    return backend()->brightnessSteps();
}

int BrightnessControl::brightnessPercent(float value) const
{
    const float maxBrightness = brightnessMax();
    if (maxBrightness <= 0) {
        return 0;
    }

    return qRound(value / maxBrightness * 100);
}

}
}
