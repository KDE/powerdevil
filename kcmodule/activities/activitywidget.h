/*
 *   SPDX-FileCopyrightText: 2011 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KSharedConfig>
#include <QWidget>

namespace KActivities
{
class Consumer;
} // namespace KActivities

namespace Ui
{
class ActivityWidget;
}

class ActivityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityWidget(const QString &activity, QWidget *parent = nullptr);
    ~ActivityWidget() override;

public Q_SLOTS:
    void load();
    void save();

    void setChanged();

Q_SIGNALS:
    void changed(bool changed);

private:
    Ui::ActivityWidget *const m_ui;
    KSharedConfig::Ptr m_profilesConfig;
    QString m_activity;
    KActivities::Consumer *const m_activityConsumer;
};
