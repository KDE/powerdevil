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
    , m_ui(new Ui::ActivityWidget)
    , m_profilesConfig(KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig | KConfig::CascadeConfig))
    , m_activity(activity)
    , m_activityConsumer(new KActivities::Consumer(this))
{
    m_ui->setupUi(this);

    for (int i = 0; i < m_ui->specialBehaviorLayout->count(); ++i) {
        QWidget *widget = m_ui->specialBehaviorLayout->itemAt(i)->widget();
        if (widget) {
            widget->setVisible(false);
            connect(m_ui->specialBehaviorRadio, &QAbstractButton::toggled, widget, &QWidget::setVisible);
        } else {
            QLayout *layout = m_ui->specialBehaviorLayout->itemAt(i)->layout();
            if (layout) {
                for (int j = 0; j < layout->count(); ++j) {
                    QWidget *widget = layout->itemAt(j)->widget();
                    if (widget) {
                        widget->setVisible(false);
                        connect(m_ui->specialBehaviorRadio, &QAbstractButton::toggled, widget, &QWidget::setVisible);
                    }
                }
            }
        }
    }

    connect(m_ui->noSettingsRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->specialBehaviorRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
}

ActivityWidget::~ActivityWidget()
{
}

void ActivityWidget::load()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    // Proper loading routine
    if (config.readEntry("mode", QString()) == "SpecialBehavior") {
        m_ui->specialBehaviorRadio->setChecked(true);
        KConfigGroup behaviorGroup = config.group("SpecialBehavior");

        m_ui->noShutdownPCBox->setChecked(behaviorGroup.readEntry("noSuspend", false));
        m_ui->noShutdownScreenBox->setChecked(behaviorGroup.readEntry("noScreenManagement", false));
    }
}

void ActivityWidget::save()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    if (m_ui->specialBehaviorRadio->isChecked()) {
        config.writeEntry("mode", "SpecialBehavior");

        KConfigGroup behaviorGroup = config.group("SpecialBehavior");

        behaviorGroup.writeEntry("noSuspend", m_ui->noShutdownPCBox->isChecked());
        behaviorGroup.writeEntry("noScreenManagement", m_ui->noShutdownScreenBox->isChecked());

        behaviorGroup.sync();
    } else {
        config.writeEntry("mode", "None");
    }

    config.sync();
}

void ActivityWidget::setChanged()
{
    Q_EMIT changed(true);
}

#include "moc_activitywidget.cpp"
