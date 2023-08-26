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
    KConfigGroup config;
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

KConfigGroup ActionConfig::configGroup() const
{
    return d->config;
}

void ActionConfig::setConfigGroup(const KConfigGroup &group)
{
    d->config = group;
}

void ActionConfig::setChanged()
{
    Q_EMIT changed();
}

}

#include "moc_powerdevilactionconfig.cpp"
