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

#pragma once

#include <powerdevilaction.h>

//#include <QDBusContext>

namespace PowerDevil
{
namespace BundledActions
{

class PlatformProfile : public PowerDevil::Action//, protected QDBusContext
{
    Q_OBJECT
    Q_DISABLE_COPY(PlatformProfile)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.PlatformProfile")

    Q_PROPERTY(QStringList profileChoices READ profileChoices CONSTANT)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY currentProfileChanged)

public:
    explicit PlatformProfile(QObject *parent);
    ~PlatformProfile() override;

    bool loadAction(const KConfigGroup &config) override;

    QStringList profileChoices() const;

    QString currentProfile() const;
    Q_SCRIPTABLE void setProfile(const QString &profile);

Q_SIGNALS:
    QString currentProfileChanged(const QString &profile);

protected:
    void onProfileLoad() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileUnload() override;
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

private:
    void readProfile();

    QStringList m_profileChoices;

    QString m_currentProfile;
    QString m_configuredProfile;

};

} // namespace BundledActions
} // namespace PowerDevil
