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

#include <QWidget>

#include <PowerDevilProfileSettings.h>

namespace Ui {
    class ActionEditWidget;
}

namespace PowerDevil
{
class ActionConfig;
}

class QCheckBox;
class KConfigGroup;

class Q_DECL_EXPORT ActionEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActionEditWidget(const QString &configName, QWidget *parent = nullptr);
    ~ActionEditWidget() override;

    void load();
    void save();

    QString configName() const;

    Q_SIGNAL void requestDefaultState();
    Q_SIGNAL void requestChangeState();

private:
    void initializeServices();
    void triggerStateRequest();

    QString m_configName;
    PowerDevilProfileSettings m_profilesConfig;
    std::unique_ptr<Ui::ActionEditWidget> ui;
};

#endif // ACTIONEDITWIDGET_H
