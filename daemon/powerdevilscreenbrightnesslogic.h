/*
 *   SPDX-FileCopyrightText: 2014 Nikita Skovoroda <chalkerx@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "powerdevilbrightnesslogic.h"

namespace PowerDevil
{
class ScreenBrightnessLogic : public BrightnessLogic
{
public:
    int toggled() const override;

protected:
    int calculateSteps(int valueMax) const override;
};

}
