/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilactionconfig.h"

namespace PowerDevil
{
class ActionConfig::Private
{
public:
    ProfileSettings *profileSettings;
};

ActionConfig::ActionConfig(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

ActionConfig::~ActionConfig()
{
    delete d;
}

PowerDevil::ProfileSettings *ActionConfig::profileSettings() const
{
    return d->profileSettings;
}

void ActionConfig::setProfileSettings(PowerDevil::ProfileSettings *settings)
{
    d->profileSettings = settings;
}

void ActionConfig::setChanged()
{
    Q_EMIT changed();
}

}

#include "moc_powerdevilactionconfig.cpp"
