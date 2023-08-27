/*
 * SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <powerdevilactionconfig.h>

class QComboBox;

namespace PowerDevil
{
namespace BundledActions
{
class PowerProfileConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    PowerProfileConfig(QObject *parent);

    void save() override;
    void load() override;
    bool enabledInProfileSettings() const override;
    void setEnabledInProfileSettings(bool enabled) override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QComboBox *m_profileCombo = nullptr;
};

} // namespace BundledActions
} // namespace PowerDevil
