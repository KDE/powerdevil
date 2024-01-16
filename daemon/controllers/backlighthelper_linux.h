/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2010 Lukas Tinkl <ltinkl@redhat.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include <QObject>
#include <QVariantAnimation>

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>
#include <kauth_version.h>

#include "backlightsysfsdevice.h"

using namespace KAuth;

class BacklightHelper : public QObject
{
    Q_OBJECT
public:
    explicit BacklightHelper(QObject *parent = nullptr);

public Q_SLOTS:
    ActionReply brightness(const QVariantMap &args);
    ActionReply brightnessmax(const QVariantMap &args);
    ActionReply setbrightness(const QVariantMap &args);
    ActionReply syspath(const QVariantMap &args);

private:
    void init();

    bool writeBrightness(int brightness) const;

    bool writeToDevice(const QString &device, int brightness) const;

    /**
     * The kernel offer from version 2.6.37 the type of the interface, and based on that
     * we can decide which interface is better for us, being the order
     * firmware-platform-raw
     */
    void initUsingBacklightType();

    bool m_isSupported = false;
    QList<BacklightSysfsDevice> m_devices;

    QVariantAnimation m_anim;
};
