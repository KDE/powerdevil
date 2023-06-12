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

#pragma once

#include <KCModule>
#include <KSharedConfig>

#include "ui_profileEditPage.h"

class ActionEditWidget;
namespace PowerDevil {
}

class ErrorOverlay;

class EditPage : public KCModule, private Ui_profileEditPage
{
    Q_OBJECT

public:
    explicit EditPage(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void onChanged(bool changed);

    void restoreDefaultProfiles();

    void notifyDaemon();

    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

    void checkAndEmitChanged();

private:
    KSharedConfig::Ptr m_profilesConfig;
    QHash< QString, bool > m_profileEdited;
    ErrorOverlay *m_errorOverlay = nullptr;
    QHash< QString, ActionEditWidget* > m_editWidgets;
};
