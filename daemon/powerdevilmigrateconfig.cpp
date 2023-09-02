/*
 *  SPDX-FileCopyrightText: Copyright 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilmigrateconfig.h"

#include <PowerDevilActivitySettings.h>

#include <KConfigGroup>
#include <KSharedConfig>

namespace PowerDevil
{

void migrateActivitiesConfig()
{
    KSharedConfigPtr profilesConfig = KSharedConfig::openConfig(QStringLiteral("powermanagementprofilesrc"));

    KConfigGroup migrationInfo = profilesConfig->group("Migration");
    if (!profilesConfig->hasGroup("Activities") || migrationInfo.hasKey("MigratedActivitiesToPlasma6")) {
        return;
    }

    // Activity special behavior config written via ActivitySettings, reading must be done manually.
    KConfigGroup oldActivitiesGroup = profilesConfig->group("Activities");

    // Every activity has its own group identified by its UUID.
    for (const QString &activityId : oldActivitiesGroup.groupList()) {
        const KConfigGroup oldConfig = oldActivitiesGroup.group(activityId);
        PowerDevil::ActivitySettings newConfig(activityId);

        if (oldConfig.readEntry("mode", "None") == "SpecialBehavior" && oldConfig.hasGroup("SpecialBehavior")) {
            const KConfigGroup oldSB = oldConfig.group("SpecialBehavior");
            newConfig.setInhibitScreenManagement(oldSB.readEntry("noScreenManagement", false));
            newConfig.setInhibitSuspend(oldSB.readEntry("noSuspend", false));
        }
        newConfig.save();
    }

    migrationInfo.writeEntry("MigratedActivitiesToPlasma6", "powerdevilrc");
    profilesConfig->sync();
}

void migrateConfig(bool isMobile, bool isVM, bool canSuspendToRam)
{
    migrateActivitiesConfig();

    // for later use in profile migration
    Q_UNUSED(isMobile);
    Q_UNUSED(isVM);
    Q_UNUSED(canSuspendToRam);
}

} // namespace PowerDevil
