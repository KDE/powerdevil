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


#ifndef POWERDEVILDPMSACTION_H
#define POWERDEVILDPMSACTION_H

#include <powerdevilaction.h>

#include <QScopedPointer>

class AbstractDpmsHelper;

class PowerDevilDPMSAction : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilDPMSAction)

public:
    explicit PowerDevilDPMSAction(QObject *parent, const QVariantList &);
    virtual ~PowerDevilDPMSAction();

protected:
    void onProfileUnload() Q_DECL_OVERRIDE;
    bool onUnloadAction() Q_DECL_OVERRIDE;
    void onWakeupFromIdle() Q_DECL_OVERRIDE;
    void onIdleTimeout(int msec) Q_DECL_OVERRIDE;
    void onProfileLoad() Q_DECL_OVERRIDE;
    void triggerImpl(const QVariantMap &args) Q_DECL_OVERRIDE;
    bool isSupported() Q_DECL_OVERRIDE;

public:
    bool loadAction(const KConfigGroup &config) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies);

private:
    void setKeyboardBrightnessHelper(int brightness);

    int m_idleTime = 0;
    PowerDevil::PolicyAgent::RequiredPolicies m_inhibitScreen = PowerDevil::PolicyAgent::None;

    int m_oldKeyboardBrightness = 0;
    QScopedPointer<AbstractDpmsHelper> m_helper;

};

#endif // POWERDEVILDPMSACTION_H
