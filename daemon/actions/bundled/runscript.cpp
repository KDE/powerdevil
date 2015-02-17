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

#include "runscript.h"

#include <QProcess>

#include <KConfigGroup>

namespace PowerDevil {
namespace BundledActions {

RunScript::RunScript(QObject* parent)
    : Action(parent)
{
    // TODO: Which policy should we enforce here? Let's go for the less restrictive one
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);
}

RunScript::~RunScript()
{

}

void RunScript::onProfileUnload()
{
    if (m_scriptPhase == 1) {
        QProcess::startDetached(m_scriptCommand);
    }
}

void RunScript::onWakeupFromIdle()
{
    //
}

void RunScript::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
    QProcess::startDetached(m_scriptCommand);
}

void RunScript::onProfileLoad()
{
    if (m_scriptPhase == 0) {
        QProcess::startDetached(m_scriptCommand);
    }
}

void RunScript::triggerImpl(const QVariantMap& args)
{
    Q_UNUSED(args);
}

bool RunScript::loadAction(const KConfigGroup& config)
{
    if (config.hasKey("scriptCommand") && config.hasKey("scriptPhase")) {
        m_scriptCommand = config.readEntry<QString>("scriptCommand", QString());
        m_scriptPhase = config.readEntry<int>("scriptPhase", 0);
        if (m_scriptPhase == 2) {
            if (!config.hasKey("idleTime")) {
                return false;
            }
            registerIdleTimeout(config.readEntry<int>("idleTime", 10000000));
        }
    }

    return true;
}

}
}
