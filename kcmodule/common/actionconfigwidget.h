/*
 *   SPDX-FileCopyrightText: 2010 Sebastian Kugler <sebas@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QMainWindow>
#include <QWidget>

#include <QGridLayout>
#include <QMap>
#include <QString>

#include "powerdevilconfigcommonprivate_export.h"

class POWERDEVILCONFIGCOMMONPRIVATE_EXPORT ActionConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActionConfigWidget(QWidget *parent);
    ~ActionConfigWidget() override;

    void addWidgets(const QList<QPair<QString, QWidget *>> &configMap);

private:
    QGridLayout *m_gridLayout;
};
