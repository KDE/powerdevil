/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "powerdevilcore_export.h" // exported just for use in tests

namespace PowerDevil
{
namespace ProfileGenerator
{

void POWERDEVILCORE_EXPORT generateProfiles(bool isMobile, bool toRam, bool toDisk);

}
}
