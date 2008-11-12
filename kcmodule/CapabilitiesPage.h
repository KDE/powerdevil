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

#ifndef CAPABILITIESPAGE_H
#define CAPABILITIESPAGE_H

#include <QWidget>

#include "ui_capabilitiesPage.h"

class CapabilitiesPage : public QWidget, private Ui_capabilitiesPage
{
    Q_OBJECT

public:
    CapabilitiesPage(QWidget *parent = 0);
    ~CapabilitiesPage();

    void fillUi();
    void load();

private slots:
    void fillCapabilities();
    void enableXSync();
    void disableScalingWarn();
    void attemptLoadingModules();

private:
    void setIssue(bool issue, const QString &text = QString(),
                  const QString &button = QString(), const QString &buticon = QString(),
                  const char *slot = 0, const QString &button2 = QString(),
                  const QString &buticon2 = QString(), const char *slot2 = 0);

    enum PollingType {
        Abstract = -1,
        WidgetBased = 1,
        XSyncBased = 2,
        TimerBased = 3
    };

signals:
    void reload();
    void reloadModule();
    void issuesFound(bool enable);
};

#endif /* CAPABILITIESPAGE_H_ */
