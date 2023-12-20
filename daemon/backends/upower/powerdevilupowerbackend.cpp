/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2010-2013 Lukáš Tinkl <ltinkl@redhat.com>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "powerdevilupowerbackend.h"

#include <powerdevil_debug.h>

#include <QDBusMessage>
#include <QDebug>
#include <QFileInfo>
#include <QPropertyAnimation>
#include <QTextStream>
#include <QTimer>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kauth_version.h>

#define HELPER_ID "org.kde.powerdevil.backlighthelper"

PowerDevilUPowerBackend::PowerDevilUPowerBackend(QObject *parent)
    : BackendInterface(parent)
{
}

PowerDevilUPowerBackend::~PowerDevilUPowerBackend() = default;

void PowerDevilUPowerBackend::init()
{
    setBackendIsReady();
}

#include "moc_powerdevilupowerbackend.cpp"
