/*
 *  SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#include "PowerButtonActionModel.h"

#include <powerdevilenums.h>
#include <powerdevilpowermanagement.h>

#include <KLocalizedString>

#include <type_traits> // std::is_same_v, std::underlying_type_t

PowerButtonActionModel::PowerButtonActionModel(QObject *parent, PowerDevil::PowerManagement *pm, std::initializer_list<PowerDevil::PowerButtonAction> actions)
    : QAbstractListModel(parent)
{
    for (PowerDevil::PowerButtonAction action : actions) {
        appendAction(action, pm);
    }
}

void PowerButtonActionModel::appendAction(PowerDevil::PowerButtonAction action, PowerDevil::PowerManagement *pm)
{
    switch (action) {
    case PowerDevil::PowerButtonAction::NoAction:
        m_data.append(Data{
            .icon = QIcon::fromTheme("dialog-cancel"),
            .name = i18n("Do nothing"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::NoAction),
        });
        break;

    case PowerDevil::PowerButtonAction::SuspendToRam:
        if (pm->canSuspend()) {
            m_data.append(Data{
                .icon = QIcon::fromTheme("system-suspend"),
                .name = i18nc("Suspend to RAM", "Sleep"),
                .value = qToUnderlying(PowerDevil::PowerButtonAction::SuspendToRam),
            });
        }
        break;

    case PowerDevil::PowerButtonAction::SuspendToDisk:
        if (pm->canHibernate()) {
            m_data.append(Data{
                .icon = QIcon::fromTheme("system-suspend-hibernate"),
                .name = i18n("Hibernate"),
                .value = qToUnderlying(PowerDevil::PowerButtonAction::SuspendToDisk),
            });
        }
        break;

    case PowerDevil::PowerButtonAction::SuspendHybrid:
        if (pm->canHybridSuspend()) {
            m_data.append(Data{
                .icon = QIcon::fromTheme("system-suspend-hybrid"),
                .name = i18n("Hybrid sleep"),
                .value = qToUnderlying(PowerDevil::PowerButtonAction::SuspendHybrid),
            });
        }
        break;

    case PowerDevil::PowerButtonAction::Shutdown:
        m_data.append(Data{
            .icon = QIcon::fromTheme("system-shutdown"),
            .name = i18nc("Power down the computer", "Shut down"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::Shutdown),
        });
        break;

    case PowerDevil::PowerButtonAction::LockScreen:
        m_data.append(Data{
            .icon = QIcon::fromTheme("system-lock-screen"),
            .name = i18n("Lock screen"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::LockScreen),
        });
        break;

    case PowerDevil::PowerButtonAction::PromptLogoutDialog:
        m_data.append(Data{
            .icon = QIcon::fromTheme("system-log-out"),
            .name = i18n("Prompt log out dialog"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::PromptLogoutDialog),
        });
        break;

    case PowerDevil::PowerButtonAction::TurnOffScreen:
        m_data.append(Data{
            .icon = QIcon::fromTheme("preferences-desktop-screensaver"),
            .name = i18n("Turn off screen"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::TurnOffScreen),
        });
        break;

    case PowerDevil::PowerButtonAction::ToggleScreenOnOff:
        m_data.append(Data{
            .icon = QIcon::fromTheme("osd-shutd-screen"),
            .name = i18n("Toggle screen on/off"),
            .value = qToUnderlying(PowerDevil::PowerButtonAction::TurnOffScreen),
        });
        break;
    }
}

QVariant PowerButtonActionModel::data(const QModelIndex &index, int role) const
{
    if (index.row() > m_data.size()) {
        return {};
    }

    switch (role) {
    case Icon:
        return m_data[index.row()].icon;
    case Name:
        return m_data[index.row()].name;
    case Value:
        static_assert(std::is_same_v<decltype(Data::value), std::underlying_type_t<PowerDevil::PowerButtonAction>>);
        return m_data[index.row()].value;
    default:
        return {};
    }
}

int PowerButtonActionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_data.size());
}

QHash<int, QByteArray> PowerButtonActionModel::roleNames() const
{
    return QHash<int, QByteArray>{{Icon, "icon"}, {Name, "name"}, {Value, "value"}};
}
