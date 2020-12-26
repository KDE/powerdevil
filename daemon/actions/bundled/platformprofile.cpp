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


#include "platformprofile.h"
#include "platformprofileadaptor.h"

#include <powerdevil_debug.h>

#include <KAuthAction>
#include <KAuthExecuteJob>
#include <KConfigGroup>

using namespace PowerDevil::BundledActions;

static const QString s_platformProfileHelperId = QStringLiteral("org.kde.powerdevil.platformprofilehelper");

PlatformProfile::PlatformProfile(QObject* parent)
    : Action(parent)
{
    new PlatformProfileAdaptor(this);

    KAuth::Action profileChoicesAction("org.kde.powerdevil.platformprofilehelper.getprofilechoices");
    profileChoicesAction.setHelperId(s_platformProfileHelperId);
    KAuth::ExecuteJob *profileChoicesJob = profileChoicesAction.execute();
    // This needs to block since we need to report isSupported immediately thereafter
    profileChoicesJob->exec();

    if (profileChoicesJob->error()) {
        // Not a warning because it might just not be supported on the given hardware/kernel version
        qCDebug(POWERDEVIL) << "Failed to get platform profile choices" << profileChoicesJob->errorText();
    } else {
        m_profileChoices = profileChoicesJob->data().value(QStringLiteral("choices")).toStringList();
        qCDebug(POWERDEVIL) << "Available platform profile choices" << m_profileChoices;

        readProfile();
    }
}

PlatformProfile::~PlatformProfile() = default;

void PlatformProfile::onProfileLoad()
{
    if (!m_configuredProfile.isEmpty()) {
        setProfile(m_configuredProfile);
    }
}

void PlatformProfile::onWakeupFromIdle()
{

}

void PlatformProfile::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
}

void PlatformProfile::onProfileUnload()
{

}

void PlatformProfile::triggerImpl(const QVariantMap &args)
{
    Q_UNUSED(args);
    // TODO move setProfile here?
    // but triggerImpl is basically just so it checks inhibitions before triggering
    // which we don't care about in this action
}

bool PlatformProfile::loadAction(const KConfigGroup &config)
{
    if (config.hasKey("profile")) {
        m_configuredProfile = config.readEntry("profile", QString());
    }

    return true;
}

bool PlatformProfile::isSupported()
{
    return !m_profileChoices.isEmpty();
}

QStringList PlatformProfile::profileChoices() const
{
    return m_profileChoices;
}

QString PlatformProfile::currentProfile() const
{
    return m_currentProfile;
}

void PlatformProfile::setProfile(const QString &profile)
{
    // TODO should we return right away if profile is not in m_profileChoices?
    // TODO should we setDelayedReply and actually forward success/failure?

    KAuth::Action profileAction("org.kde.powerdevil.platformprofilehelper.setprofile");
    profileAction.setHelperId(s_platformProfileHelperId);
    profileAction.addArgument("profile", profile);
    KAuth::ExecuteJob *profileJob = profileAction.execute();
    connect(profileJob, &KJob::result, this, [this, profileJob, profile/*, msg*/] {
        if (profileJob->error()) {
            qCWarning(POWERDEVIL) << "Failed to set platform profile" << profileJob->errorText();
            /*QDBusConnection::sessionBus().send(
                msg.createErrorReply(
                    QStringLiteral("org.kde.Solid.PowerManagement.Actions.PlatformProfile.SetProfileFailed"),
                    profileJob->errorText()));*/
            return;
        }

        qCDebug(POWERDEVIL) << "Successfully set platform profile to" << profile;

        // TODO if we had better error reporting in the helper we could just assume if
        // this call worked, that the new profile is what we just asked it to change it to
        readProfile();

        //QDBusConnection::sessionBus().send(msg.createReply());
    });
    profileJob->start();
}

void PlatformProfile::readProfile()
{
    KAuth::Action profileAction("org.kde.powerdevil.platformprofilehelper.getprofile");
    profileAction.setHelperId(s_platformProfileHelperId);
    KAuth::ExecuteJob *profileJob = profileAction.execute();
    connect(profileJob, &KJob::result, this, [this, profileJob] {
        if (profileJob->error()) {
            qCWarning(POWERDEVIL) << "Failed to read current platform profile" << profileJob->errorText();
            return;
        }

        const QString profile = profileJob->data().value(QStringLiteral("profile")).toString();

        if (m_currentProfile != profile) {
            qCDebug(POWERDEVIL) << "Platform profile changed to" << profile;
            m_currentProfile = profile;
            Q_EMIT currentProfileChanged(profile);
        }
    });
    profileJob->start();
}
