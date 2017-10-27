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


#ifndef POWERDEVIL_BUNDLEDACTIONS_DIMDISPLAY_H
#define POWERDEVIL_BUNDLEDACTIONS_DIMDISPLAY_H

#include <powerdevilaction.h>


namespace PowerDevil {
namespace BundledActions {

class DimDisplay : public PowerDevil::Action
{
    Q_OBJECT
    Q_DISABLE_COPY(DimDisplay)

public:
    explicit DimDisplay(QObject *parent);
    virtual ~DimDisplay() = default;

protected:
    void onProfileUnload() Q_DECL_OVERRIDE;
    void onWakeupFromIdle() Q_DECL_OVERRIDE;
    void onIdleTimeout(int msec) Q_DECL_OVERRIDE;
    void onProfileLoad() Q_DECL_OVERRIDE;
    void triggerImpl(const QVariantMap& args) Q_DECL_OVERRIDE;
    bool isSupported() Q_DECL_OVERRIDE;

public:
    bool loadAction(const KConfigGroup& config) Q_DECL_OVERRIDE;

private:
    void setBrightnessHelper(int screen, int keyboard, bool force = false);

    int m_dimOnIdleTime = 0;

    int m_oldScreenBrightness = 0;
    int m_oldKeyboardBrightness = 0;

    bool m_dimmed = false;
};

}

}

#endif // POWERDEVIL_BUNDLEDACTIONS_DIMDISPLAY_H
