/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class QSlider;
class QLabel;

namespace PowerDevil::BundledActions
{
class KeyboardBrightnessControlConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    KeyboardBrightnessControlConfig(QObject *);

    void save() override;
    void load() override;
    bool enabledInProfileSettings() const override;
    void setEnabledInProfileSettings(bool enabled) override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QSlider *m_slider;
    QLabel *m_text;
};

}
