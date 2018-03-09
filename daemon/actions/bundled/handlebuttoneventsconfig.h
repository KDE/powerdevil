/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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

#ifndef POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTSCONFIG_H
#define POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTSCONFIG_H

#include <powerdevilactionconfig.h>

class QComboBox;
class QCheckBox;
namespace PowerDevil {
namespace BundledActions {

class HandleButtonEventsConfig : public PowerDevil::ActionConfig
{
    Q_OBJECT
    Q_DISABLE_COPY(HandleButtonEventsConfig)

public:
    HandleButtonEventsConfig(QObject* parent, const QVariantList&);
    ~HandleButtonEventsConfig() override;

    void save() override;
    void load() override;
    QList< QPair< QString, QWidget* > > buildUi() override;

private:
    QComboBox *m_lidCloseCombo;
    QCheckBox *m_triggerLidActionWhenExternalMonitorPresent;
    QComboBox *m_powerButtonCombo;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTSCONFIG_H
