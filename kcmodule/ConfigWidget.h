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

#include "GeneralPage.h"
#include "AssignmentPage.h"
#include "EditPage.h"
#include "CapabilitiesPage.h"

class ConfigWidget : public QWidget, private Ui_powerDevilConfig
{
        Q_OBJECT

    public:
        ConfigWidget( QWidget *parent = 0 );
        ~ConfigWidget();

    public slots:
        void load();
        void save();

    signals:
        void changed( bool change );
        void reloadRequest();
        void reloadModule();

    private:
        GeneralPage *m_generalPage;
        AssignmentPage *m_assignmentPage;
        EditPage *m_editPage;
        CapabilitiesPage *m_capabilitiesPage;
};

#endif /*CONFIGWIDGET_H*/
