/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "externalbrightnesscontrol.h"
#include "displaybrightness.h"

static constexpr uint32_t s_version = 2;

ExternalBrightnessController::ExternalBrightnessController()
    : QWaylandClientExtensionTemplate<ExternalBrightnessController, &QtWayland::kde_external_brightness_v1::destroy>(s_version)
{
}

void ExternalBrightnessController::setDisplays(const QList<DisplayBrightness *> &displays)
{
    if (!isActive()) {
        m_waylandObjects.clear();
        return;
    }
    std::erase_if(m_waylandObjects, [&displays](const auto &pair) {
        const auto &[display, waylandObj] = pair;
        return !displays.contains(display);
    });
    for (DisplayBrightness *display : displays) {
        if (!m_waylandObjects.contains(display)) {
            m_waylandObjects.emplace(display, std::make_unique<ExternalBrightnessControl>(this, display));
        }
    }
}

ExternalBrightnessControl::ExternalBrightnessControl(ExternalBrightnessController *controller, DisplayBrightness *display)
    : QtWayland::kde_external_brightness_device_v1(controller->create_brightness_control())
    , m_display(display)
{
    set_internal(display->isInternal() ? 1 : 0);
    if (auto data = display->edidData()) {
        set_edid(QString::fromStdString(data->toBase64().toStdString()));
    }
    set_max_brightness(display->maxBrightness());
    set_observed_brightness(m_display->brightness());
    commit();
    connect(display, &DisplayBrightness::externalBrightnessChangeObserved, this, [this]() {
        set_max_brightness(m_display->maxBrightness());
        set_observed_brightness(m_display->brightness());
        commit();
    });
}

ExternalBrightnessControl::~ExternalBrightnessControl()
{
    destroy();
}

void ExternalBrightnessControl::kde_external_brightness_device_v1_requested_brightness(uint32_t value)
{
    m_display->setBrightness(value, false);
}

#include "moc_externalbrightnesscontrol.cpp"
