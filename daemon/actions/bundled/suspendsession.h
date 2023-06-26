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

#pragma once

#include <powerdevilaction.h>

#include <QScopedPointer>

namespace PowerDevil
{
class KWinKScreenHelperEffect;

namespace BundledActions
{
class SuspendSession : public PowerDevil::Action
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.SuspendSession")

public:
    enum Mode {
        None = 0,
        ToRamMode = 1,
        ToDiskMode = 2,
        SuspendHybridMode = 4,
        ShutdownMode = 8,
        LogoutDialogMode = 16,
        LockScreenMode = 32,
        TurnOffScreenMode = 64,
        ToggleScreenOnOffMode = 128,
    };

    explicit SuspendSession(QObject *parent);
    ~SuspendSession() override;

    bool loadAction(const KConfigGroup &config) override;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;

public Q_SLOTS:
    void suspendToRam();
    void suspendToDisk();
    void suspendHybrid();

Q_SIGNALS:
    void aboutToSuspend();
    void resumingFromSuspend();

private Q_SLOTS:
    void triggerSuspendSession(uint action);

private:
    bool m_suspendThenHibernateEnabled = false;
    int m_idleTime = 0;
    uint m_autoType;
    QVariantMap m_savedArgs;
    QScopedPointer<PowerDevil::KWinKScreenHelperEffect> m_fadeEffect;
};

}

}
