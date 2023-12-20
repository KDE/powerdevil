/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilbackendinterface.h"
#include "brightnessosdwidget.h"
#include "powerdevil_debug.h"
#include <QDebug>

namespace PowerDevil
{

BackendInterface::BackendInterface(QObject *parent)
    : QObject(parent)
{
}

BackendInterface::~BackendInterface()
{
}

void BackendInterface::setBackendIsReady()
{
    Q_EMIT backendReady();
}
}

#include "moc_powerdevilbackendinterface.cpp"
