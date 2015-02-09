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

template <typename T> using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

namespace PowerDevil {
    class KWinKScreenHelperEffect;
}

class PowerDevilDPMSAction : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilDPMSAction)

public:
    explicit PowerDevilDPMSAction(QObject *parent, const QVariantList &);
    virtual ~PowerDevilDPMSAction() = default;

protected:
    virtual void onProfileUnload();
    virtual bool onUnloadAction();
    virtual void onWakeupFromIdle();
    virtual void onIdleTimeout(int msec);
    virtual void onProfileLoad();
    virtual void triggerImpl(const QVariantMap &args);
    bool isSupported();

public:
    virtual bool loadAction(const KConfigGroup &config);

private Q_SLOTS:
    void onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies);

private:
    void setKeyboardBrightnessHelper(int brightness);

    bool m_supported = false;

    int m_idleTime = 0;
    PowerDevil::PolicyAgent::RequiredPolicies m_inhibitScreen = PowerDevil::PolicyAgent::None;

    int m_oldKeyboardBrightnessValue = 0;
    QScopedPointer<PowerDevil::KWinKScreenHelperEffect> m_fadeEffect;

};

#endif // POWERDEVILDPMSACTION_H
