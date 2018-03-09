/***************************************************************************
 *   Copyright (C) 2010 by Sebastian Kugler <sebas@kde.org>                *
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

#ifndef ACTIONCONFIG_H
#define ACTIONCONFIG_H

#include <QMainWindow>
#include <QWidget>

#include <QGridLayout>
#include <QMap>
#include <QString>


class Q_DECL_EXPORT ActionConfigWidget : public QWidget
{
Q_OBJECT
public:
    explicit ActionConfigWidget(QWidget* parent);
    ~ActionConfigWidget();

    void addWidgets(QList<QPair <QString, QWidget*> > configMap);

private:
    QGridLayout* m_gridLayout;
};

#endif // ActionConfigWidget_H
