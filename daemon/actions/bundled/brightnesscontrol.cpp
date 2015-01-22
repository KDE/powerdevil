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

#include <powerdevilbackendinterface.h>
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

    KGlobalAccel *accel = KGlobalAccel::self();
    KActionCollection* actionCollection = new KActionCollection( this );

    QAction* globalAction = actionCollection->addAction("Increase Screen Brightness");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Increase Screen Brightness"));
    accel->setGlobalShortcut(globalAction, Qt::Key_MonBrightnessUp);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(increaseBrightness()));

    globalAction = actionCollection->addAction("Decrease Screen Brightness");
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Decrease Screen Brightness"));
    accel->setGlobalShortcut(globalAction, Qt::Key_MonBrightnessDown);
    connect(globalAction, SIGNAL(triggered(bool)), SLOT(decreaseBrightness()));
}

BrightnessControl::~BrightnessControl()
{
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
    // if the current profile is more conservative than the previous one and the
    // current brightness is lower than the new profile
    if (((m_currentProfile == "Battery" && m_lastProfile == "AC") ||
         (m_currentProfile == "LowBattery" && (m_lastProfile == "AC" || m_lastProfile == "Battery"))) &&
        m_defaultValue > brightness()) {

        // We don't want to change anything here
        qCDebug(POWERDEVIL) << "Not changing brightness, the current one is lower and the profile is more conservative";
    } else if (m_defaultValue >= 0) {
        QVariantMap args;
        args["Value"] = QVariant::fromValue((float)m_defaultValue);

        // plugging in/out the AC is always explicit
        if ((m_currentProfile == "AC" && m_lastProfile != "AC") ||
            (m_currentProfile != "AC" && m_lastProfile == "AC")) {
            args["Explicit"] = true;
        }

        trigger(args);
    }
}

void BrightnessControl::triggerImpl(const QVariantMap& args)
{
    if (args.contains("Step")) {
        backend()->setBrightnessStep(args["Step"].toInt());
    } else if ((QMetaType::Type) args["Value"].type() == QMetaType::Int) {
        backend()->setBrightnessValue(args["Value"].toInt());
    } else {
        backend()->setBrightness(args["Value"].toFloat());
    }
    if (args["Explicit"].toBool() && !args["Silent"].toBool()) {
        BrightnessOSDWidget::show(brightness());
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
    m_lastProfile = m_currentProfile;
    m_currentProfile = config.parent().name();

    qCDebug(POWERDEVIL) << "Profiles: " << m_currentProfile << m_lastProfile;

    if (config.hasKey("value")) {
        m_defaultValue = config.readEntry<int>("value", 50);
    } else {
        m_defaultValue = -1;
    }

    return true;
}

void BrightnessControl::onBrightnessChangedFromBackend(const BrightnessLogic::BrightnessInfo &info, BackendInterface::BrightnessControlType type)
{
    if (type == BackendInterface::Screen) {
        int brightness = qRound(info.percentage);
        Q_EMIT brightnessValueChanged(info.value);
        Q_EMIT brightnessStepChanged(info.step);
        Q_EMIT brightnessChanged(brightness);
    }
}

int BrightnessControl::brightness() const
{
    return qRound(backend()->brightness());
}

void BrightnessControl::setBrightness(int percent)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<float>((float)percent);
    args["Explicit"] = true;
    trigger(args);
}

void BrightnessControl::setBrightnessSilent(int percent)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<float>((float)percent);
    args["Explicit"] = true;
    args["Silent"] = true;
    trigger(args);
}

void BrightnessControl::increaseBrightness()
{
    backend()->brightnessKeyPressed(BrightnessLogic::Increase);
    BrightnessOSDWidget::show(brightness());
}

void BrightnessControl::decreaseBrightness()
{
    backend()->brightnessKeyPressed(BrightnessLogic::Decrease);
    BrightnessOSDWidget::show(brightness());
}

int BrightnessControl::brightnessValue() const
{
    return backend()->brightnessValue();
}

int BrightnessControl::brightnessValueMax() const
{
    return backend()->brightnessValueMax();
}

void BrightnessControl::setBrightnessValue(int value)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<int>(value);
    args["Explicit"] = true;
    trigger(args);
}

void BrightnessControl::setBrightnessValueSilent(int value)
{
    QVariantMap args;
    args["Value"] = QVariant::fromValue<int>(value);
    args["Explicit"] = true;
    args["Silent"] = true;
    trigger(args);
}

int BrightnessControl::brightnessStep() const
{
    return backend()->brightnessStep();
}

int BrightnessControl::brightnessStepMax() const
{
    return backend()->brightnessStepMax();
}

void BrightnessControl::setBrightnessStep(int step)
{
    QVariantMap args;
    args["Step"] = QVariant::fromValue<int>(step);
    args["Explicit"] = true;
    trigger(args);
}

}
}
