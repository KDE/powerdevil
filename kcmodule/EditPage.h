/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kde.org>                      *
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

#ifndef EDITPAGE_H
#define EDITPAGE_H

#include <QWidget>

#include "ui_profileEditPage.h"

class KToolBar;

class EditPage : public QWidget, private Ui_profileEditPage
{
    Q_OBJECT

public:
    EditPage(QWidget *parent = 0);
    ~EditPage();
    void fillUi();

    void load();
    void save();

signals:
    void changed(bool ch);
    void profilesChanged();

private slots:
    void emitChanged();
    void setProfileChanged();

    void enableSaveProfile();
    void enableBoxes();

    void loadProfile();
    void saveProfile(const QString &p = QString());
    void switchProfile(QListWidgetItem *current, QListWidgetItem *previous);
    void reloadAvailableProfiles();
    void createProfile(const QString &name, const QString &icon);
    void editProfile(const QString &prevname, const QString &icon);
    void deleteCurrentProfile();
    void createProfile();
    void editProfile();

    void importProfiles();
    void exportProfiles();

    void openUrl(const QString &url);

private:
    enum IdleAction {
        None = 0,
        Standby = 1,
        S2Ram = 2,
        S2Disk = 4,
        Shutdown = 8,
        Lock = 16,
        ShutdownDialog = 32,
        TurnOffScreen = 64
    };

    KSharedConfig::Ptr m_profilesConfig;
    bool m_profileEdited;
    KToolBar *m_toolBar;
};

#endif /* EDITPAGE_H */
