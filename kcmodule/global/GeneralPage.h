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

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include <KCModule>

#include "ui_generalPage.h"

class ErrorOverlay;
class GeneralPage : public KCModule, private Ui_generalPage
{
    Q_OBJECT

public:
    GeneralPage(QWidget *parent, const QVariantList &args);
    virtual ~GeneralPage();
    void fillUi();

    void load();
    void save();
    virtual void defaults();

private slots:
    void configureNotifications();
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

private:
    QWeakPointer< ErrorOverlay > m_errorOverlay;
};

#endif /* GENERALPAGE_H */
