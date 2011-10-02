/***************************************************************************
 *   Copyright (C) 2008-2011 by Dario Freddi <drf@kde.org>                 *
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


#ifndef ACTIONEDITWIDGET_H
#define ACTIONEDITWIDGET_H

#include <QtGui/QWidget>

#include <KSharedConfig>

namespace PowerDevil
{
class ActionConfig;
}

class QCheckBox;
class KConfigGroup;

class ActionEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActionEditWidget(const QString &configName, QWidget *parent = 0);
    virtual ~ActionEditWidget();

    QString configName() const;

public Q_SLOTS:
    void load();
    void save();

private Q_SLOTS:
    void onChanged();

Q_SIGNALS:
    void changed(bool changed);

private:
    QString m_configName;
    KSharedConfig::Ptr m_profilesConfig;
    QHash< QString, QCheckBox* > m_actionsHash;
    QHash< QString, PowerDevil::ActionConfig* > m_actionsConfigHash;
};

#endif // ACTIONEDITWIDGET_H
