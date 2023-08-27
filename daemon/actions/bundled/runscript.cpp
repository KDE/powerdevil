/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "runscript.h"
#include "powerdevil_debug.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <QProcess>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::RunScript, "powerdevilrunscriptaction.json")

namespace PowerDevil::BundledActions
{
RunScript::RunScript(QObject *parent)
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

void RunScript::onIdleTimeout(std::chrono::milliseconds timeout)
{
    Q_UNUSED(timeout);
    runCommand();
}

void RunScript::onProfileLoad(const QString & /*previousProfile*/, const QString & /*newProfile*/)
{
    if (m_scriptPhase == 0) {
        runCommand();
    }
}

void RunScript::triggerImpl(const QVariantMap &args)
{
    Q_UNUSED(args);
}

bool RunScript::loadAction(const KConfigGroup &config)
{
    if (config.hasKey("scriptCommand") && config.hasKey("scriptPhase")) {
        m_scriptCommand = config.readEntry<QString>("scriptCommand", QString());
        m_scriptPhase = config.readEntry<int>("scriptPhase", 0);
        if (m_scriptPhase == 2) {
            if (!config.hasKey("idleTime")) {
                return false;
            }
            registerIdleTimeout(std::chrono::milliseconds(config.readEntry<int>("idleTime", 10000000)));
        }
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

#include "runscript.moc"

#include "moc_runscript.cpp"
