/***************************************************************************
 *   Copyright (C) 2011 by Dario Freddi <drf@kde.org>                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/


#include "activitywidget.h"

#include "ui_activityWidget.h"

#include "../daemon/actions/bundled/suspendsession.h"

#include "powerdevilpowermanagement.h"

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
    connect(m_ui->alwaysActionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_ui->alwaysAfterSpin, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));
}

ActivityWidget::~ActivityWidget()
{

}

void ActivityWidget::load()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    using namespace PowerDevil::BundledActions;

    m_ui->alwaysActionBox->clear();
    if (PowerDevil::PowerManagement::instance()->canSuspend()) {
        m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-suspend"),
                                       i18nc("Suspend to RAM", "Sleep"), (uint)SuspendSession::ToRamMode);
    }
    if (PowerDevil::PowerManagement::instance()->canHibernate()) {
        m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-suspend-hibernate"),
                                       i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
    }
    m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), (uint)SuspendSession::ShutdownMode);

    // Proper loading routine
    if (config.readEntry("mode", QString()) == "SpecialBehavior") {
        m_ui->specialBehaviorRadio->setChecked(true);
        KConfigGroup behaviorGroup = config.group("SpecialBehavior");

        m_ui->noShutdownPCBox->setChecked(behaviorGroup.readEntry("noSuspend", false));
        m_ui->noShutdownScreenBox->setChecked(behaviorGroup.readEntry("noScreenManagement", false));
        m_ui->alwaysBox->setChecked(behaviorGroup.readEntry("performAction", false));

        KConfigGroup actionConfig = behaviorGroup.group("ActionConfig");
        m_ui->alwaysActionBox->setCurrentIndex(m_ui->alwaysActionBox->findData(actionConfig.readEntry("suspendType", 0)));
        m_ui->alwaysAfterSpin->setValue(actionConfig.readEntry("idleTime", 600000) / 60 / 1000);
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
        behaviorGroup.writeEntry("performAction", m_ui->alwaysBox->isChecked());

        KConfigGroup actionConfig = behaviorGroup.group("ActionConfig");
        actionConfig.writeEntry("suspendType", m_ui->alwaysActionBox->itemData(m_ui->alwaysActionBox->currentIndex()));
        actionConfig.writeEntry("idleTime", m_ui->alwaysAfterSpin->value() * 60 * 1000);

        actionConfig.sync();
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
