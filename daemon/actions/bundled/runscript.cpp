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
#include "powerdevil_debug.h"

#include <QProcess>

#include <PowerDevilProfileSettings.h>

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
        runCommand();
    }
}

void RunScript::onWakeupFromIdle()
{
    //
}

void RunScript::onIdleTimeout(int msec)
{
    Q_UNUSED(msec);
    runCommand();
}

void RunScript::onProfileLoad()
{
    if (m_scriptPhase == 0) {
        runCommand();
    }
}

void RunScript::triggerImpl(const QVariantMap& args)
{
    Q_UNUSED(args);
}

bool RunScript::loadAction(const PowerDevilProfileSettings &settings)
{
    m_scriptCommand = settings.runScriptCommand();
    m_scriptPhase = settings.runScriptPhase();

    if (m_scriptPhase == PowerDevilEnums::RunScriptMode::After) {
        int idleTime = settings.runScriptIdleTimeMsec();
        if (idleTime == 0) {
            return false;
        }
        registerIdleTimeout(idleTime);
    }

    return true;
}

void RunScript::runCommand()
{
    bool success;

    QStringList args = QProcess::splitCommand(m_scriptCommand);
    if (args.isEmpty()) {
        qCWarning(POWERDEVIL) << "Empty command?";
        return;
    }

    QProcess process;
    process.setProgram(args.takeFirst());
    process.setArguments(args);
    success = process.startDetached();

    if (!success) {
        qCWarning(POWERDEVIL) << "Failed to run" << m_scriptCommand;
    }
}

}
}
