/*
    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "PowerProfileModel.h"

#include <powerdevilenums.h>

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>

using namespace Qt::StringLiterals;

PowerProfileModel::PowerProfileModel(QObject *parent)
    : QAbstractListModel(parent)
{
    /* Loads the values for the Power Profile modes */
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                      QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                      QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                      QStringLiteral("profileChoices"));

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            qWarning() << "Failed to query platform profile choices" << reply.error().message();
            return;
        }

        const PowerProfileModel::Data profiles[] = {
            {.name = i18nc("@option:combobox Power profile", "Power Save"), .iconName = u"battery-profile-powersave"_s, .value = u"power-saver"_s},
            {.name = i18nc("@option:combobox Power profile", "Balanced"), .iconName = u"battery-profile-balanced"_s, .value = u"balanced"_s},
            {.name = i18nc("@option:combobox Power profile", "Performance"), .iconName = u"battery-profile-performance"_s, .value = u"performance"_s}};

        beginResetModel();
        m_data.clear();
        m_data.append(PowerProfileModel::Data{.name = i18n("Leave unchanged"), .iconName = u"dialog-cancel-symbolic"_s, .value = QString()});

        const QStringList availableProfiles = reply.value();
        for (const auto &profile : profiles) {
            if (availableProfiles.contains(profile.value)) {
                m_data.append(profile);
            }
        }
        endResetModel();
    });
}

QVariant PowerProfileModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_data.size()) {
        return {};
    }

    const auto &item = m_data.at(index.row());

    switch (role) {
    case Name:
        return item.name;
    case IconName:
        return item.iconName;
    case Value:
        return item.value;
    }

    return {};
}

int PowerProfileModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_data.size());
}

QHash<int, QByteArray> PowerProfileModel::roleNames() const
{
    return QHash<int, QByteArray>{{Name, "name"_ba}, {IconName, "iconName"_ba}, {Value, "value"_ba}};
}

#include "moc_PowerProfileModel.cpp"
