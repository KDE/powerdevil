/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class QSpinBox;
class KComboBox;
class KUrlRequester;

namespace PowerDevil::BundledActions
{
class RunScriptConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    RunScriptConfig(QObject *parent);

    void save() override;
    void load() override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    KUrlRequester *m_urlRequester;
    KComboBox *m_comboBox;
    QSpinBox *m_idleTime;

private Q_SLOTS:
    void onIndexChanged(const QString &);
};

}
