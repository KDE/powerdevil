/*
 *   SPDX-FileCopyrightText: 2011 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>

#include "powerdevilconfigcommonprivate_export.h"

class POWERDEVILCONFIGCOMMONPRIVATE_EXPORT ErrorOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorOverlay(QWidget *baseWidget, const QString &details, QWidget *parent = nullptr);
    ~ErrorOverlay() override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void reposition();

private:
    QWidget *m_BaseWidget;
};
