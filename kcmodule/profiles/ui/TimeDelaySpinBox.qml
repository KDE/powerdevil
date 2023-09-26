/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.SpinBox {
    id: root

    readonly property string minutesSuffixes: i18nc("List of recognized strings for 'minutes' in a time delay expression such as 'after 10 min'", "m|min|mins|minutes")
    readonly property string secondsSuffixes: i18nc("List of recognized strings for 'seconds' in a time delay expression such as 'after 10 sec'", "s|sec|secs|seconds")
    readonly property var extractor: new RegExp(i18nc("Validator/extractor regular expression for a time delay number and unit, from e.g. 'after 10 min'. Uses recognized strings for minutes and seconds as %1 and %2.",
                                                      "[^\\d]*(\\d+)\\s*(%1|%2)\\s*", minutesSuffixes, secondsSuffixes))

    editable: true
    validator: RegularExpressionValidator {
        regularExpression: extractor
    }

    textFromValue: function(value, locale) {
        if (stepSize == 60 && value % 60 == 0) {
            return i18np("after %1 min", "after %1 min", value / 60);
        }
        return i18np("after %1 sec", "after %1 sec", value);
    }

    valueFromText: function(text, locale) {
        const match = text.match(root.extractor);
        if (match[1]) {
            const multiplier = minutesSuffixes.split("|").includes(match[2]) ? 60 : 1;
            return Number.fromLocaleString(locale, match[1]) * multiplier;
        }
        return root.value; // unchanged
    }

    TextMetrics {
        id: metricsMax
        text: textFromValue(to, root.locale)
    }
    TextMetrics {
        id: metricsAlmostMax
        text: textFromValue(to - stepSize, root.locale)
    }
    implicitWidth: Math.max(metricsMax.advanceWidth, metricsAlmostMax.advanceWidth) + Kirigami.Units.gridUnit * 2
}
