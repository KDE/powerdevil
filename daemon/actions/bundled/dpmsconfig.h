/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class QSpinBox;
class PowerDevilDPMSActionConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    PowerDevilDPMSActionConfig(QObject *parent);

    void save() override;
    void load() override;
    bool enabledInProfileSettings() const override;
    void setEnabledInProfileSettings(bool enabled) override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QSpinBox *m_spinBox;
    QSpinBox *m_spinIdleTimeoutLocked;
};
