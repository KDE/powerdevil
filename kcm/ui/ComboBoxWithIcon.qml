/*
 * SPDX-FileCopyrightText: 2020 Tobias Fella <fella@posteo.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

QQC2.ComboBox {
    id: comboBox

    required model

    Kirigami.StyleHints.iconName: model.index(currentIndex, 0).data(model.KItemModels.KRoleNames.role("iconName")) ?? ""

    delegate: QQC2.ItemDelegate {
        required property string index

        // model roles from roleNames(), expose these in your QAbstractItemModel if you haven't already
        required property string name
        required property string iconName

        text: name
        highlighted: index === currentIndex
        icon.name: iconName
    }
}
