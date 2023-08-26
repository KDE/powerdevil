/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilactionconfig.h>

class QSpinBox;

namespace PowerDevil::BundledActions
{
class DimDisplayConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
public:
    DimDisplayConfig(QObject *);

    void save() override;
    void load() override;
    QList<QPair<QString, QWidget *>> buildUi() override;

private:
    QSpinBox *m_spinBox;
};

}
