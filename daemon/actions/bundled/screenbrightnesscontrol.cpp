/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
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
    return core()->screenBrightnessController()->isSupported();
}

bool ScreenBrightnessControl::loadAction(const PowerDevil::ProfileSettings &profileSettings)
{
    Q_UNUSED(profileSettings)

    return false;
}

void ScreenBrightnessControl::onDisplayAdded(const QString &displayId)
{
    Q_EMIT DisplayAdded(displayId);
}

void ScreenBrightnessControl::onDisplayRemoved(const QString &displayId)
{
    Q_EMIT DisplayRemoved(displayId);
}

void ScreenBrightnessControl::onBrightnessChanged(const QString &displayId,
                                                  const BrightnessLogic::BrightnessInfo &info,
                                                  const QString &sourceClientName,
                                                  const QString &sourceClientContext)
{
    Q_EMIT BrightnessChanged(displayId, info.value, sourceClientName, sourceClientContext);
}

void ScreenBrightnessControl::onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &info)
{
    Q_EMIT BrightnessRangeChanged(displayId, info.valueMin, info.valueMax, info.value);
}

QStringList ScreenBrightnessControl::displayIds() const
{
    return core()->screenBrightnessController()->displayIds();
}

QString ScreenBrightnessControl::label(const QString &displayId) const
{
    return core()->screenBrightnessController()->label(displayId);
}

int ScreenBrightnessControl::brightness(const QString &displayId) const
{
    return core()->screenBrightnessController()->brightness(displayId);
}

int ScreenBrightnessControl::maxBrightness(const QString &displayId) const
{
    return core()->screenBrightnessController()->maxBrightness(displayId);
}

int ScreenBrightnessControl::minBrightness(const QString &displayId) const
{
    return core()->screenBrightnessController()->minBrightness(displayId);
}

int ScreenBrightnessControl::knownSafeMinBrightness(const QString &displayId) const
{
    // Theoretically we should provide a change signal for this property, because it can change as
    // display connections change, even if it's constant per individual display. In practice,
    // this won't change as long as laptop screens don't get added or removed at runtime.
    // Which shouldn't happen.
    return core()->screenBrightnessController()->knownSafeMinBrightness(displayId);
}

int ScreenBrightnessControl::preferredNumberOfBrightnessSteps(const QString &displayId) const
{
    return core()->screenBrightnessController()->brightnessSteps(displayId);
}

void ScreenBrightnessControl::setBrightness(const QString &displayId, int value, const QString &sourceClientContext, uint flags)
{
    QString sourceClientName = connection().baseService();
    core()->screenBrightnessController()->setBrightness(displayId, value, sourceClientName, sourceClientContext);

    if (!(flags & static_cast<uint>(SetBrightnessFlags::SuppressIndicator))) {
        BrightnessOSDWidget::show(brightnessPercent(displayId, value));
    }
}

int ScreenBrightnessControl::brightnessPercent(const QString &displayId, float value) const
{
    const float max = maxBrightness(displayId);
    if (max <= 0) {
        return 0;
    }

    return qRound(value / max * 100);
}

}

#include "screenbrightnesscontrol.moc"

#include "moc_screenbrightnesscontrol.cpp"
