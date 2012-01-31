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


#ifndef ACTIVITYWIDGET_H
#define ACTIVITYWIDGET_H

#include <QtGui/QWidget>
#include <KSharedConfig>

class ActionEditWidget;
namespace KActivities
{
class Consumer;
} // namespace KActivities

namespace Ui {
class ActivityWidget;
}

class ActivityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityWidget(const QString &activity, QWidget *parent = 0);
    virtual ~ActivityWidget();

public Q_SLOTS:
    void load();
    void save();

    void setChanged();

Q_SIGNALS:
    void changed(bool changed);

private:
    Ui::ActivityWidget *m_ui;
    KSharedConfig::Ptr m_profilesConfig;
    QString m_activity;
    KActivities::Consumer *m_activityConsumer;
    ActionEditWidget* m_actionEditWidget;
};

#endif // ACTIVITYWIDGET_H
