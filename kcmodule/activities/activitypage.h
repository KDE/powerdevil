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

#include <kworkspace/kactivityconsumer.h>

class ErrorOverlay;
class ActivityWidget;
class KActivityConsumer;
class KMessageWidget;
class ActivityPage : public KCModule
{
    Q_OBJECT

public:
    ActivityPage(QWidget *parent, const QVariantList &args);
    virtual ~ActivityPage();
    void fillUi();

    void load();
    void save();
    virtual void defaults();

private Q_SLOTS:
    void onActivityServiceStatusChanged(KActivityConsumer::ServiceStatus status);
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

private:
    KActivityConsumer *m_activityConsumer;
    QList< ActivityWidget* > m_activityWidgets;
    QWeakPointer< ErrorOverlay > m_errorOverlay;
    QWeakPointer< KMessageWidget > m_messageWidget;
    KActivityConsumer::ServiceStatus m_previousServiceStatus;
};

#endif // ACTIVITYPAGE_H
