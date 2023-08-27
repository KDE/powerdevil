/*
 *   SPDX-FileCopyrightText: 2008 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KCModule>
#include <KSharedConfig>

#include "ui_profileEditPage.h"

class ActionEditWidget;
class ErrorOverlay;

class EditPage : public KCModule, private Ui_profileEditPage
{
    Q_OBJECT

public:
    explicit EditPage(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void onChanged(bool changed);

    void notifyDaemon();

    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);

    void checkAndEmitChanged();

private:
    QHash<QString, bool> m_profileEdited;
    ErrorOverlay *m_errorOverlay = nullptr;
    QHash<QString, ActionEditWidget *> m_editWidgets;
};
