/***************************************************************************
 *   Copyright (C) 2011 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/


#ifndef ACTIVITYPAGE_H
#define ACTIVITYPAGE_H

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
    ActivityPage(QWidget *parent, const QVariantList &args);
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

    QTabWidget *m_tabWidget;

    KActivities::Consumer *m_activityConsumer;
    QList< ActivityWidget* > m_activityWidgets;
    ErrorOverlay *m_errorOverlay = nullptr;
    KMessageWidget *m_messageWidget = nullptr;
    KActivities::Consumer::ServiceStatus m_previousServiceStatus;
};

#endif // ACTIVITYPAGE_H
