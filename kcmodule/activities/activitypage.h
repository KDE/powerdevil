/*
 *   SPDX-FileCopyrightText: 2011 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KCModule>

#include <KActivities/Consumer>

class QTabWidget;

class KMessageWidget;

class ErrorOverlay;
class ActivityWidget;

class ActivityPage : public KCModule
{
    Q_OBJECT

public:
    ActivityPage(QObject *parent, const KPluginMetaData &data);
    ~ActivityPage() override;
    void fillUi();

    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void onActivityServiceStatusChanged(KActivities::Consumer::ServiceStatus status);
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

private:
    void populateTabs();

    QTabWidget *m_tabWidget = nullptr;

    KActivities::Consumer *const m_activityConsumer;
    QList<ActivityWidget *> m_activityWidgets;
    ErrorOverlay *m_errorOverlay = nullptr;
    KMessageWidget *m_messageWidget = nullptr;
    KActivities::Consumer::ServiceStatus m_previousServiceStatus;
};
