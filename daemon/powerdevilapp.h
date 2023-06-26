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

#pragma once

#include <QGuiApplication>
#include <QVariantList>

namespace PowerDevil
{
class Core;
}

using InhibitionInfo = QPair<QString, QString>;

class PowerDevilApp : public QGuiApplication
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilApp)

public:
    explicit PowerDevilApp(int &argc, char **argv);
    ~PowerDevilApp() override;

    void init();

private Q_SLOTS:
    void onCoreReady();

private:
    PowerDevil::Core *m_core;
};
