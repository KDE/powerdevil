/*
    SPDX-FileCopyrightText: 2016 Sebastian Kügler <sebas@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@broulik.de>

    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>
    SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie.clarius@kde.org>


    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "osdaction.h"

#include <KLocalizedString>

using namespace PowerDevil;

QList<OsdAction> OsdAction::availableActions(QStringList availableProfiles)
{
    QList<OsdAction> actions;
    if (availableProfiles.contains(QStringLiteral("power-saver"))) {
        actions << OsdAction(QStringLiteral("power-saver"),
                             i18ndc("powerdevil", "power profile", "Power Save Mode"),
                             QStringLiteral("battery-profile-powersave"));
    }
    if (availableProfiles.contains(QStringLiteral("balanced"))) {
        actions << OsdAction(QStringLiteral("balanced"), i18ndc("powerdevil", "power profile", "Balanced Performance Mode"), QStringLiteral("speedometer"));
    }
    if (availableProfiles.contains(QStringLiteral("performance"))) {
        actions << OsdAction(QStringLiteral("performance"),
                             i18ndc("powerdevil", "power profile", "Maximum Performance Mode"),
                             QStringLiteral("battery-profile-performance"));
    }
    return actions;
}

#include "moc_osdaction.cpp"
