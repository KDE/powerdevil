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

#include "powerprofileconfig.h"

#include "powerdevilpowermanagement.h"

#include <QComboBox>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_FACTORY(PowerDevilPowerProfileConfigFactory, registerPlugin<PowerDevil::BundledActions::PowerProfileConfig>();)

using namespace PowerDevil::BundledActions;

PowerProfileConfig::PowerProfileConfig(QObject *parent, const QVariantList &args)
    : ActionConfig(parent)
{
    Q_UNUSED(args)
}

PowerProfileConfig::~PowerProfileConfig() = default;

void PowerProfileConfig::save()
{
    const QString profile = m_profileCombo->currentData().toString();
    configGroup().writeEntry("profile", profile);

    configGroup().sync();
}

void PowerProfileConfig::load()
{
    configGroup().config()->reparseConfiguration();

    const QString profile = configGroup().readEntry("profile", QString());
    if (m_profileCombo) {
        m_profileCombo->setCurrentIndex(qMax(0, m_profileCombo->findData(profile)));
    }
}

QList<QPair<QString, QWidget *>> PowerProfileConfig::buildUi()
{
    m_profileCombo = new QComboBox;
    // Uniform ComboBox width throughout all action config modules
    m_profileCombo->setMinimumWidth(300);
    m_profileCombo->setMaximumWidth(300);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PowerProfileConfig::setChanged);

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                      QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                      QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                      QStringLiteral("profileChoices"));

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), m_profileCombo);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();

        m_profileCombo->clear();
        m_profileCombo->addItem(i18n("Leave unchanged"));

        if (reply.isError()) {
            qWarning() << "Failed to query platform profile choices" << reply.error().message();
            return;
        }

        const QHash<QString, QString> profileNames = {
            {QStringLiteral("power-saver"), i18n("Power Save")},
            {QStringLiteral("balanced"), i18n("Balanced")},
            {QStringLiteral("performance"), i18n("Performance")},
        };

        const QStringList choices = reply.value();
        for (const QString &choice : choices) {
            m_profileCombo->addItem(profileNames.value(choice, choice), choice);
        }

        if (configGroup().isValid()) {
            const QString profile = configGroup().readEntry("profile", QString());
            m_profileCombo->setCurrentIndex(qMax(0, m_profileCombo->findData(profile)));
        }
    });


    return {qMakePair<QString, QWidget *>(i18nc("Switch to power management profile", "Switch to:"), m_profileCombo)};
}

#include "powerprofileconfig.moc"
