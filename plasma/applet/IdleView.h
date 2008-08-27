/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#ifndef IDLEVIEW_H
#define IDLEVIEW_H

namespace Plasma
{
class Applet;
class Icon;
}

#include "AbstractView.h"

#include <QtDBus/QDBusConnection>

class QGraphicsLinearLayout;
class QGraphicsProxyWidget;
class KComboBox;
class KPushButton;

class IdleView : public AbstractView
{
    Q_OBJECT

public:
    IdleView(Plasma::Applet *parent, QDBusConnection dbs);
    virtual ~IdleView();

    void installPackageFromFile(const QString &filename);

private slots:
    void updateBatteryIcon(int percent, bool plugged);
    void updateProfiles(const QString &current, const QStringList &available);
    void setSelectedProfile();


private:
    QDBusConnection m_dbus;
    QGraphicsLinearLayout *m_layout;
    QGraphicsLinearLayout *m_actionsLayout;

    KComboBox *m_profilesCombo;
    QGraphicsProxyWidget *m_comboBox;

    KPushButton *m_profilesButton;
    QGraphicsProxyWidget *m_button;

    Plasma::Icon *m_batteryIcon;

    QGraphicsLinearLayout *m_lineLayout;

};

#endif /*IDLEVIEW_H*/
