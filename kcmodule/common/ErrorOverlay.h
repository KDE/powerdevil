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

#ifndef ERROROVERLAY_H
#define ERROROVERLAY_H

#include <QWidget>

class Q_DECL_EXPORT ErrorOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorOverlay(QWidget *baseWidget, const QString &details, QWidget *parent = 0);
    virtual ~ErrorOverlay();

protected:
    bool eventFilter(QObject *object, QEvent *event) Q_DECL_OVERRIDE;

private:
    void reposition();

private:
    QWidget *m_BaseWidget;
};

#endif // ERROROVERLAY_H
