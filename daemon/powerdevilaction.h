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


#ifndef POWERDEVIL_POWERDEVILACTION_H
#define POWERDEVIL_POWERDEVILACTION_H

#include "powerdevilpolicyagent.h"

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

#include <kdemacros.h>

class KConfigGroup;

namespace PowerDevil
{
class BackendInterface;
class Core;

class KDE_EXPORT Action : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Action)

public:
    explicit Action(QObject *parent);
    virtual ~Action();

    virtual bool loadAction(const KConfigGroup &config) = 0;
    virtual bool unloadAction();

    void trigger(const QVariantMap &args);

protected:
    void registerIdleTimeout(int msec);
    void setRequiredPolicies(PowerDevil::PolicyAgent::RequiredPolicies requiredPolicies);

    virtual void triggerImpl(const QVariantMap &args) = 0;

    PowerDevil::BackendInterface *backend();
    PowerDevil::Core *core();

protected Q_SLOTS:
    virtual void onProfileLoad() = 0;
    virtual void onIdleTimeout(int msec) = 0;
    virtual void onWakeupFromIdle() = 0;
    virtual void onProfileUnload() = 0;
    virtual bool onUnloadAction();

Q_SIGNALS:
    void actionTriggered(bool result, const QString &error = QString());

private:
    class Private;
    Private * const d;

    friend class Core;
    friend class ActionPool;
};

}

#endif // POWERDEVIL_POWERDEVILACTION_H
