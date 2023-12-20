/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
    SPDX-FileCopyrightText: 2010 Alejandro Fiestas <alex@eyeos.org>
    SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <powerdevilbackendinterface.h>

#include <QDBusConnection>

#include <map>
#include <memory>

#include "powerdevilupowerbackend_export.h"

#define UPOWER_SERVICE "org.freedesktop.UPower"

class QPropertyAnimation;
class QTimer;
class DDCutilBrightness;
class POWERDEVILUPOWERBACKEND_EXPORT PowerDevilUPowerBackend : public PowerDevil::BackendInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(PowerDevilUPowerBackend)
    Q_PLUGIN_METADATA(IID "org.kde.powerdevil.upowerbackend");

public:
    explicit PowerDevilUPowerBackend(QObject *parent = nullptr);
    ~PowerDevilUPowerBackend() override;

    void init() override;
};
