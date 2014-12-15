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


#ifndef POWERDEVIL_BUNDLEDACTIONS_SUSPENDSESSION_H
#define POWERDEVIL_BUNDLEDACTIONS_SUSPENDSESSION_H

#include <powerdevilaction.h>

class QDBusPendingCallWatcher;

namespace PowerDevil
{
namespace BundledActions
{

class SuspendSession : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(SuspendSession)
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
        TurnOffScreenMode = 64
    };

    explicit SuspendSession(QObject *parent);
    virtual ~SuspendSession();

    virtual bool loadAction(const KConfigGroup& config);
    
protected:
    virtual void onProfileUnload();
    virtual void onWakeupFromIdle();
    virtual void onIdleTimeout(int msec);
    virtual void onProfileLoad();
    virtual void triggerImpl(const QVariantMap& args);

public Q_SLOTS:
    void suspendToRam();
    void suspendToDisk();
    void suspendHybrid();

    void onResumeFromSuspend();

Q_SIGNALS:
    void aboutToSuspend();
    void resumingFromSuspend();

private slots:
    void lockCompleted();
    void triggerSuspendSession(uint action);

private:
    uint m_autoType;
    QVariantMap m_savedArgs;
    QDBusPendingCallWatcher *m_dbusWatcher;

    void lockScreenAndWait();
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_SUSPENDSESSION_H
