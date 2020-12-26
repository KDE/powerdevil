/*
 * Copyright 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "platformprofilehelper.h"

#include <powerdevil_debug.h>

#include <QFile>
#include <QFileInfo>

static const QString s_acpiSysFsPath = QStringLiteral("/sys/firmware/acpi");

static const QString s_platformProfile = QStringLiteral("platform_profile");
static const QString s_platformProfileChoices = QStringLiteral("platform_profile_choices");

PlatformProfileHelper::PlatformProfileHelper(QObject *parent)
    : QObject(parent)
{
}


ActionReply PlatformProfileHelper::getprofilechoices(const QVariantMap &args)
{
    Q_UNUSED(args);

    const QString platformChoicesPath = s_acpiSysFsPath + QLatin1Char('/') + s_platformProfileChoices;

    if (!QFileInfo::exists(platformChoicesPath)) {
        // FIXME for debugging
        /*ActionReply rrr;
        rrr.setData({
            {QStringLiteral("choices"), QStringList{QStringLiteral("low-power"),QStringLiteral("cool"),QStringLiteral("quiet"),QStringLiteral("balanced"),QStringLiteral("performance")}}
        });
        return rrr;*/

        auto reply = ActionReply::HelperErrorReply();
        // TODO i18n?
        reply.setErrorDescription(QStringLiteral("Platform profiles not supported"));
        return reply;
    }

    QFile choicesFile(platformChoicesPath);
    if (!choicesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to read platform profile choices"));
        return reply;
    }

    const QStringList choices = QString::fromUtf8(choicesFile.readLine()).split(QLatin1Char(' '), Qt::SkipEmptyParts);

    ActionReply reply;
    reply.setData({
        {QStringLiteral("choices"), choices}
    });
    return reply;
}

ActionReply PlatformProfileHelper::getprofile(const QVariantMap &args)
{
    Q_UNUSED(args);

    QFile profileFile(s_acpiSysFsPath + QLatin1Char('/') + s_platformProfile);
    if (!profileFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // FIXME for testing
        /*ActionReply rrr;
        rrr.setData({
            {QStringLiteral("profile"), QStringLiteral("cool")}
        });
        return rrr;*/

        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to read current platform profile"));
        return reply;
    }

    const QString profile = QString::fromUtf8(profileFile.readLine());

    ActionReply reply;
    reply.setData({
        {QStringLiteral("profile"), profile}
    });
    return reply;
}

ActionReply PlatformProfileHelper::setprofile(const QVariantMap &args)
{
    const QString profile = args.value(QStringLiteral("profile")).toString();

    QFile profileFile(s_acpiSysFsPath + QLatin1Char('/') + s_platformProfile);
    if (!profileFile.open(QIODevice::WriteOnly)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to open current platform profile for writing"));
        return reply;
    }

    // TODO can we tell when it rejected the write because the profile isn't supported/valid?
    // or we just read the file after we've written to verify
    // then on the caller side we could just asusme that setting worked when the helper succeeded
    if (profileFile.write(profile.toUtf8()) == -1) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write current platform profile"));
        return reply;
    }

    return ActionReply::SuccessReply();
}

KAUTH_HELPER_MAIN("org.kde.powerdevil.platformprofilehelper", PlatformProfileHelper)
