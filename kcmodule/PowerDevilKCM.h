/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#ifndef POWERDEVILKCM_H
#define POWERDEVILKCM_H

#include <kcmodule.h>
#include <QDBusConnection>

class ConfigWidget;

class PowerDevilKCM : public KCModule
{
    Q_OBJECT

public:
    PowerDevilKCM(QWidget *parent, const QVariantList &args);

    void load();
    void save();
    void defaults();

private:
    ConfigWidget *m_widget;
    QDBusConnection m_dbus;
};

#endif /*POWERDEVILKCM_H*/
