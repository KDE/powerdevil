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

#include "ConfigWidget.h"

#include "PowerDevilSettings.h"

ConfigWidget::ConfigWidget(QWidget *parent)
        : KPageWidget(parent),
        m_generalPage(new GeneralPage(this)),
        m_editPage(new EditPage(this)),
        m_capabilitiesPage(new CapabilitiesPage(this))
{
    layout()->setMargin(0);
    m_generalPage->layout()->setMargin(0);
    m_editPage->layout()->setMargin(0);
    m_capabilitiesPage->layout()->setMargin(0);

    KPageWidgetItem *general = addPage(m_generalPage, i18n("General Settings"));
    KPageWidgetItem *edit = addPage(m_editPage, i18n("Edit Profiles"));
    KPageWidgetItem *capab = addPage(m_capabilitiesPage, i18n("Capabilities"));

    general->setHeader("");
    edit->setHeader("");
    capab->setHeader("");

    general->setIcon(KIcon("configure"));
    edit->setIcon(KIcon("edit-select-all"));
    capab->setIcon(KIcon("hwinfo"));

    connect(m_generalPage, SIGNAL(changed(bool)), SIGNAL(changed(bool)));
    connect(m_editPage, SIGNAL(changed(bool)), SIGNAL(changed(bool)));

    connect(m_editPage, SIGNAL(profilesChanged()), SIGNAL(reloadRequest()));
    connect(m_editPage, SIGNAL(profilesChanged()), m_generalPage, SLOT(reloadAvailableProfiles()));

    connect(m_capabilitiesPage, SIGNAL(reload()), SIGNAL(reloadRequest()));
    connect(m_capabilitiesPage, SIGNAL(reloadModule()), SIGNAL(reloadModule()));
    connect(m_capabilitiesPage, SIGNAL(issuesFound(bool)), m_generalPage, SLOT(enableIssue(bool)));
}

ConfigWidget::~ConfigWidget()
{
}

void ConfigWidget::load()
{
    m_generalPage->load();
    m_editPage->load();
    m_capabilitiesPage->load();
}

void ConfigWidget::save()
{
    m_generalPage->save();
    m_editPage->save();
}

#include "ConfigWidget.moc"
