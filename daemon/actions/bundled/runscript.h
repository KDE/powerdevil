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


#ifndef POWERDEVIL_BUNDLEDACTIONS_RUNSCRIPT_H
#define POWERDEVIL_BUNDLEDACTIONS_RUNSCRIPT_H

#include <powerdevilaction.h>


namespace PowerDevil {
namespace BundledActions {

class RunScript : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(RunScript)

public:
    explicit RunScript(QObject* parent);
    ~RunScript() override;

protected:
    void onProfileUnload() override;
    void onWakeupFromIdle() override;
    void onIdleTimeout(int msec) override;
    void onProfileLoad() override;
    void triggerImpl(const QVariantMap& args) override;

public:
    bool loadAction(const KConfigGroup& config) override;

private:
    void runCommand();
    int m_scriptPhase;
    QString m_scriptCommand;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_RUNSCRIPT_H
