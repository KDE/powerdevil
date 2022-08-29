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
#include "brightnessvalue.h"

#include <QScopedPointer>

class AbstractDpmsHelper;

namespace KScreen {
    class Dpms;
}

namespace PowerDevil {
namespace BundledActions {

class DPMS : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(DPMS)

public:
    explicit DPMS(QObject *parent, const QVariantList &);
    ~DPMS() override;

Q_SIGNALS:
    void startFade();
    void stopFade();

protected:
    void onProfileUnload() override {}
    bool onUnloadAction() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override {}
    void triggerImpl(const QVariantMap &args) override;
    bool isSupported() override;

public:
    bool loadAction(const KConfigGroup &config) override;

private Q_SLOTS:
    void onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies);

private:
    void setKeyboardBrightnessHelper(PerceivedBrightness brightness);

    int m_idleTime = 0;
    PowerDevil::PolicyAgent::RequiredPolicies m_inhibitScreen = PowerDevil::PolicyAgent::None;

    PerceivedBrightness m_oldKeyboardBrightness = 0_pb;
    QScopedPointer<KScreen::Dpms> m_dpms;

    bool m_lockBeforeTurnOff = false;
    void lockScreen();

};

}
}

#endif // POWERDEVILDPMSACTION_H
