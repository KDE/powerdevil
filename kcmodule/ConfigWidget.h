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

#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include "ui_dialog.h"

class KConfig;

class ConfigWidget : public QWidget, private Ui_powerDevilConfig
{
        Q_OBJECT

    public:
        ConfigWidget( QWidget *parent = 0 );

        void fillUi();

        void load();
        void save();

    signals:
        void changed( bool ch );
        void profilesChanged();

    private slots:
        void emitChanged();
        void setProfileChanged();

        void enableSaveProfile();
        void enableBoxes();

        void loadProfile();
        void saveProfile( const QString &p = QString() );
        void switchProfile( QListWidgetItem *current, QListWidgetItem *previous );
        void reloadAvailableProfiles();
        void createProfile( const QString &name );
        void deleteCurrentProfile();
        void createProfile();

        void fillCapabilities();

        void importProfiles();
        void exportProfiles();

    private:
        enum IdleAction {
            Shutdown = 1,
            S2Disk = 2,
            S2Ram = 3,
            Standby = 4,
            Lock = 5,
            None = 0
        };

        KConfig *m_profilesConfig;
        bool m_profileEdited;
};

#endif /*CONFIGWIDGET_H*/
