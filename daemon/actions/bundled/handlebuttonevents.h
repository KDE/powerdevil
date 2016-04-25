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


#ifndef POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H
#define POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H

#include <KScreen/Types>

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

namespace PowerDevil {
namespace BundledActions {

class HandleButtonEvents : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(HandleButtonEvents)
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.HandleButtonEvents")

public:
    explicit HandleButtonEvents(QObject* parent);
    virtual ~HandleButtonEvents();

    virtual bool loadAction(const KConfigGroup& config);
    virtual bool isSupported();

Q_SIGNALS:
    void triggersLidActionChanged(bool triggers);

protected:
    virtual void triggerImpl(const QVariantMap& args);
    virtual void onProfileUnload();
    virtual void onWakeupFromIdle();
    virtual void onIdleTimeout(int msec);
    virtual void onProfileLoad();

public Q_SLOTS:
    int lidAction() const;
    bool triggersLidAction() const;

private Q_SLOTS:
    void onButtonPressed(PowerDevil::BackendInterface::ButtonType type);
    void powerOffButtonTriggered();
    void suspendToRam();
    void suspendToDisk();

    void checkOutputs();

private:
    void processAction(uint action);
    void triggerAction(const QString &action, const QVariant &type);

    KScreen::ConfigPtr m_screenConfiguration;
    uint m_lidAction = 0;
    bool m_triggerLidActionWhenExternalMonitorPresent = false;
    bool m_externalMonitorPresent = false;

    uint m_powerButtonAction = 0;
    uint m_sleepButtonAction = 1;
    uint m_hibernateButtonAction = 2;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H
