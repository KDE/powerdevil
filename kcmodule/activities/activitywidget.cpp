/*
 *   SPDX-FileCopyrightText: 2011 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "activitywidget.h"

#include "ui_activityWidget.h"

#include <KActivities/Consumer>
#include <KConfigGroup>
#include <QLayout>
#include <Solid/Battery>
#include <Solid/Device>

ActivityWidget::ActivityWidget(const QString &activity, QWidget *parent)
    : QWidget(parent)
    , m_activitySettings(activity)
    , m_ui(new Ui::ActivityWidget)
    , m_activity(activity)
    , m_activityConsumer(new KActivities::Consumer(this))
{
    m_ui->setupUi(this);

    connect(m_ui->inhibitSuspendCheckBox, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->inhibitScreenManagementCheckBox, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
}

ActivityWidget::~ActivityWidget()
{
}

void ActivityWidget::load()
{
    m_activitySettings.load();

    m_ui->inhibitSuspendCheckBox->setChecked(m_activitySettings.inhibitSuspend());
    m_ui->inhibitScreenManagementCheckBox->setChecked(m_activitySettings.inhibitScreenManagement());

    Q_EMIT changed(false);
}

void ActivityWidget::save()
{
    m_activitySettings.setInhibitSuspend(m_ui->inhibitSuspendCheckBox->isChecked());
    m_activitySettings.setInhibitScreenManagement(m_ui->inhibitScreenManagementCheckBox->isChecked());

    m_activitySettings.save();
}

void ActivityWidget::setChanged()
{
    Q_EMIT changed(true);
}

#include "moc_activitywidget.cpp"
