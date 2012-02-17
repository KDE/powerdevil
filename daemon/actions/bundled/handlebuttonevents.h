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


#ifndef POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H
#define POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

namespace PowerDevil {
namespace BundledActions {

class HandleButtonEvents : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(HandleButtonEvents)

public:
    explicit HandleButtonEvents(QObject* parent);
    virtual ~HandleButtonEvents();

    virtual bool loadAction(const KConfigGroup& config);

protected:
    virtual void triggerImpl(const QVariantMap& args);
    virtual void onProfileUnload();
    virtual void onWakeupFromIdle();
    virtual void onIdleTimeout(int msec);
    virtual void onProfileLoad();

private Q_SLOTS:
    void onButtonPressed(PowerDevil::BackendInterface::ButtonType type);

private:
    void processAction(uint action, bool isExplicit);
    void triggerAction(const QString &action, const QVariant &type, bool isExplicit);

    uint m_lidAction;
    uint m_powerButtonAction;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_HANDLEBUTTONEVENTS_H
