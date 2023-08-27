/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class KComboBox;
class QComboBox;
class QCheckBox;
class QSpinBox;

namespace PowerDevil::BundledActions
{
class SuspendSessionConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    SuspendSessionConfig(QObject *parent);

    void save() override;
    void load() override;
    bool enabledInProfileSettings() const override;
    void setEnabledInProfileSettings(bool enabled) override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QSpinBox *m_idleTime;
    KComboBox *m_comboBox;
    QComboBox *m_sleepModeCombo;
};

}
