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

#include <QAction>
#include <QDebug>

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::BrightnessControl, "powerdevilbrightnesscontrolaction.json")

namespace PowerDevil {
namespace BundledActions {
BrightnessControl::BrightnessControl(QObject *parent, const QVariantList &)
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
    const PerceivedBrightness absoluteBrightnessValue(qRound(m_defaultValue / 100.0 * brightnessMax()));

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
    } else if (absoluteBrightnessValue >= 0_pb) {
        QVariantMap args{
            {QStringLiteral("Value"), (int)absoluteBrightnessValue}
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
    const PerceivedBrightness value(args.value(QStringLiteral("Value")).toInt());

    backend()->setBrightness(value, BackendInterface::BrightnessControlType::Screen);
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
        Q_EMIT brightnessChanged((int)PerceivedBrightness(info.value, brightnessMax()));
        Q_EMIT brightnessMaxChanged(info.valueMax);
    }
}

PerceivedBrightness BrightnessControl::perceivedBrightness() const
{
    return backend()->perceivedBrightness(BackendInterface::Screen);
}

int BrightnessControl::brightness() const
{
    return (int)backend()->perceivedBrightness(BackendInterface::Screen);
}

int BrightnessControl::brightnessMax() const
{
    return backend()->brightnessMax(BackendInterface::Screen);
}

void BrightnessControl::setBrightness(int value)
{
    trigger({
        {QStringLiteral("Value"), value},
        {QStringLiteral("Explicit"), true}
    });
}

void BrightnessControl::setBrightnessSilent(int value)
{
    trigger({
        {QStringLiteral("Value"), value},
        {QStringLiteral("Explicit"), true},
        {QStringLiteral("Silent"), true}
    });
}

void BrightnessControl::increaseBrightness()
{
    const PerceivedBrightness newBrightness = PerceivedBrightness(backend()->brightnessKeyPressed(BrightnessLogic::Increase, BackendInterface::Screen), brightnessMax());
    if (newBrightness >= 0_pb) {
        BrightnessOSDWidget::show(brightnessPercent(newBrightness));
    }
}

void BrightnessControl::decreaseBrightness()
{
    const PerceivedBrightness newBrightness = PerceivedBrightness(backend()->brightnessKeyPressed(BrightnessLogic::Decrease, BackendInterface::Screen), brightnessMax());
    if (newBrightness >= 0_pb) {
        BrightnessOSDWidget::show(brightnessPercent(newBrightness));
    }
}

int BrightnessControl::brightnessSteps() const
{
    return (int)backend()->brightnessSteps(BackendInterface::Screen);
}

PerceivedBrightness BrightnessControl::brightnessPercent(PerceivedBrightness value) const
{
    const float maxBrightness = brightnessMax();
    if (maxBrightness <= 0) {
        return 0_pb;
    }

    return PerceivedBrightness(qRound((int)value / maxBrightness * 100));
}

}
}
#include "brightnesscontrol.moc"
