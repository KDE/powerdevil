/*
 *   SPDX-FileCopyrightText: 2008 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KCModule>

#include "ui_generalPage.h"

namespace PowerDevil
{
class GlobalSettings;
}

class ErrorOverlay;

class GeneralPage : public KCModule, private Ui_generalPage
{
    Q_OBJECT

public:
    GeneralPage(QObject *parent, const KPluginMetaData &data);
    ~GeneralPage() override;
    void fillUi();

    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void configureNotifications();
    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);
    void onChargeStopThresholdChanged(int threshold);

private:
    void setChargeThresholdSupported(bool supported);

    ErrorOverlay *m_errorOverlay = nullptr;
    PowerDevil::GlobalSettings *m_settings;

    int m_chargeStartThreshold = 0;
    int m_chargeStopThreshold = 100;
};
