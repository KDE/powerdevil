/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class QComboBox;
class QCheckBox;
namespace PowerDevil::BundledActions
{
class HandleButtonEventsConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT

public:
    HandleButtonEventsConfig(QObject *parent);

    void save() override;
    void load() override;
    bool enabledInProfileSettings() const override;
    void setEnabledInProfileSettings(bool enabled) override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QComboBox *m_lidCloseCombo;
    QCheckBox *m_triggerLidActionWhenExternalMonitorPresent;
    QComboBox *m_powerButtonCombo;
};

}
