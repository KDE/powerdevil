/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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


#ifndef POWERDEVIL_POWERDEVILACTIONCONFIG_H
#define POWERDEVIL_POWERDEVILACTIONCONFIG_H

#include <QtGui/QWidget>

#include <KConfigGroup>

#include <kdemacros.h>

namespace PowerDevil {

class KDE_EXPORT ActionConfig : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ActionConfig)

public:
    ActionConfig(QObject *parent);
    virtual ~ActionConfig();

    KConfigGroup configGroup() const;
    void setConfigGroup(const KConfigGroup &group);

    virtual QList< QPair< QString, QWidget* > > buildUi() = 0;

    virtual void load() = 0;
    virtual void save() = 0;

protected Q_SLOTS:
    void setChanged();

Q_SIGNALS:
    void changed();

private:
    class Private;
    Private * const d;
};

}

#endif // POWERDEVIL_POWERDEVILACTIONCONFIG_H
