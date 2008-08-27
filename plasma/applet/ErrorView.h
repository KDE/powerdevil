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

#ifndef ERRORVIEW_H
#define ERRORVIEW_H

namespace Plasma
{
class Applet;
class Icon;
}

#include "AbstractView.h"

class QGraphicsLinearLayout;
class QGraphicsProxyWidget;

class ErrorView : public AbstractView
{
    Q_OBJECT

public:
    ErrorView(Plasma::Applet *parent, const QString &message);
    virtual ~ErrorView();

private slots:
    void launchKcm();

private:
    QGraphicsLinearLayout *m_layout;
    QGraphicsProxyWidget *m_proxyErrorLabel;
    QGraphicsProxyWidget *m_proxyLaunchButton;
    Plasma::Icon *m_icon;
};

#endif /*ERRORVIEW_H*/
