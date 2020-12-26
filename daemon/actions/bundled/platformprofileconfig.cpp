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

#include "platformprofileconfig.h"

#include "powerdevilpowermanagement.h"

#include <QComboBox>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_FACTORY(PowerDevilPlatformProfileConfigFactory, registerPlugin<PowerDevil::BundledActions::PlatformProfileConfig>(); )

using namespace PowerDevil::BundledActions;

PlatformProfileConfig::PlatformProfileConfig(QObject *parent, const QVariantList &args)
    : ActionConfig(parent)
{
    Q_UNUSED(args)
}

PlatformProfileConfig::~PlatformProfileConfig() = default;

void PlatformProfileConfig::save()
{
    const QString profile = m_profileCombo->currentData().toString();
    configGroup().writeEntry("profile", profile);

    configGroup().sync();
}

void PlatformProfileConfig::load()
{
    configGroup().config()->reparseConfiguration();

    const QString profile = configGroup().readEntry("profile", QString());

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.Solid.PowerManagement"),
        QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PlatformProfile"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get")
    );
    msg.setArguments({
        QStringLiteral("org.kde.Solid.PowerManagement.Actions.PlatformProfile"),
        QStringLiteral("profileChoices")
    });

    auto *watcher = new QDBusPendingCallWatcher( QDBusConnection::sessionBus().asyncCall(msg), this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, profile](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QVariant> reply = *watcher;
        watcher->deleteLater();

        m_profileCombo->clear();
        m_profileCombo->addItem(i18n("Leave unchanged"));

        if (reply.isError()) {
            qWarning() << "Failed to query platform profile choices" << reply.error().message();
            return;
        }

        const QStringList choices = reply.value().toStringList();
        for (const QString &choice : choices) {
            // TODO translated title and perhaps an icon
            m_profileCombo->addItem(choice, choice);
        }
        m_profileCombo->setCurrentIndex(qMax(0, m_profileCombo->findData(profile)));
    });
}

QList<QPair<QString, QWidget *>> PlatformProfileConfig::buildUi()
{
    m_profileCombo = new QComboBox;
    // Uniform ComboBox width throughout all action config modules
    m_profileCombo->setMinimumWidth(300);
    m_profileCombo->setMaximumWidth(300);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlatformProfileConfig::setChanged);

    return {
        qMakePair<QString, QWidget *>(i18nc("Switch to power management profile", "Switch to:"), m_profileCombo)
    };
}

#include "platformprofileconfig.moc"
