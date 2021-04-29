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


#ifndef POWERDEVILDPMSACTIONCONFIG_H
#define POWERDEVILDPMSACTIONCONFIG_H

#include <powerdevilactionconfig.h>

class QSpinBox;
class PowerDevilDPMSActionConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilDPMSActionConfig)
public:
    PowerDevilDPMSActionConfig(QObject* parent, const QVariantList&);
    ~PowerDevilDPMSActionConfig() override;

    void save() override;
    void load() override;
    QList< QPair< QString, QWidget* > > buildUi() override;

private:
    QSpinBox *m_spinBox;
};

#endif // POWERDEVILDPMSACTIONCONFIG_H
