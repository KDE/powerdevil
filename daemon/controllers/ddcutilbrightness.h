/*  This file is part of the KDE project
 *    SPDX-FileCopyrightText: 2017 Dorian Vogel <dorianvogel@gmail.com>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 *
 */

#pragma once

#include <QObject>
#include <QStringList>

#ifdef WITH_DDCUTIL
#include <ddcutil_c_api.h>
#endif

#include "ddcutildisplay.h"

#include <unordered_map>

class DDCutilBrightness : public QObject
{
    Q_OBJECT
public:
    explicit DDCutilBrightness(QObject *parent = nullptr);
    ~DDCutilBrightness();

    void detect();
    QStringList displayIds() const;
    bool isSupported() const;
    int brightness(const QString &displayId);
    int maxBrightness(const QString &displayId);
    void setBrightness(const QString &displayId, int value);

private:
#ifdef WITH_DDCUTIL
    QString generateDisplayId(const DDCA_Display_Info &displayInfo) const;
#endif

private:
    QStringList m_displayIds;
    std::unordered_map<QString, std::unique_ptr<DDCutilDisplay>> m_displays;
};
