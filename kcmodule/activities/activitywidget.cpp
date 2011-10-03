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

#include "daemon/actions/bundled/suspendsession.h"

#include <kworkspace/kactivityconsumer.h>
#include <Solid/PowerManagement>
#include <actioneditwidget.h>
#include <QLayout>

ActivityWidget::ActivityWidget(const QString& activity, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ActivityWidget)
    , m_profilesConfig(KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig))
    , m_activity(activity)
    , m_activityConsumer(new KActivityConsumer(this))
    , m_actionEditWidget(new ActionEditWidget(QString("Activities/%1/SeparateSettings").arg(activity)))
{
    m_ui->setupUi(this);

    m_ui->separateSettingsLayout->addWidget(m_actionEditWidget);

    for (int i = 0; i < m_ui->specialBehaviorLayout->count(); ++i) {
        QWidget *widget = m_ui->specialBehaviorLayout->itemAt(i)->widget();
        if (widget) {
            widget->setVisible(false);
            connect(m_ui->specialBehaviorRadio, SIGNAL(toggled(bool)), widget, SLOT(setVisible(bool)));
        } else {
            QLayout *layout = m_ui->specialBehaviorLayout->itemAt(i)->layout();
            if (layout) {
                for (int j = 0; j < layout->count(); ++j) {
                    QWidget *widget = layout->itemAt(j)->widget();
                    if (widget) {
                        widget->setVisible(false);
                        connect(m_ui->specialBehaviorRadio, SIGNAL(toggled(bool)), widget, SLOT(setVisible(bool)));
                    }
                }
            }
        }
    }

    m_actionEditWidget->setVisible(false);
    m_actionEditWidget->load();

    connect(m_ui->separateSettingsRadio, SIGNAL(toggled(bool)), m_actionEditWidget, SLOT(setVisible(bool)));

    connect(m_ui->actLikeRadio, SIGNAL(toggled(bool)), this, SLOT(setChanged()));
    connect(m_ui->noSettingsRadio, SIGNAL(toggled(bool)), this, SLOT(setChanged()));
    connect(m_ui->separateSettingsRadio, SIGNAL(toggled(bool)), this, SLOT(setChanged()));
    connect(m_ui->specialBehaviorRadio, SIGNAL(toggled(bool)), this, SLOT(setChanged()));
    connect(m_ui->actLikeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_ui->alwaysActionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_ui->alwaysAfterSpin, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    connect(m_actionEditWidget, SIGNAL(changed(bool)), this, SIGNAL(changed(bool)));
}

ActivityWidget::~ActivityWidget()
{

}

void ActivityWidget::load()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    using namespace PowerDevil::BundledActions;

    QSet< Solid::PowerManagement::SleepState > methods = Solid::PowerManagement::supportedSleepStates();

    if (methods.contains(Solid::PowerManagement::SuspendState)) {
        m_ui->alwaysActionBox->addItem(KIcon("system-suspend"),
                                       i18n("Sleep"), (uint)SuspendSession::ToRamMode);
    }
    if (methods.contains(Solid::PowerManagement::HibernateState)) {
        m_ui->alwaysActionBox->addItem(KIcon("system-suspend-hibernate"),
                                       i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
    }
    m_ui->alwaysActionBox->addItem(KIcon("system-shutdown"), i18n("Shutdown"), (uint)SuspendSession::ShutdownMode);

    m_ui->actLikeComboBox->clear();

    m_ui->actLikeComboBox->addItem(KIcon("battery-charging"), i18n("PC running on AC power"), "AC");
    m_ui->actLikeComboBox->addItem(KIcon("battery-060"), i18n("PC running on battery power"), "Battery");
    m_ui->actLikeComboBox->addItem(KIcon("battery-low"), i18n("PC running on low battery"), "LowBattery");

    foreach (const QString &activity, m_activityConsumer->listActivities()) {
        if (activity == m_activity) {
            continue;
        }

        if (activitiesGroup.group(activity).readEntry("mode", "None") == "None" ||
            activitiesGroup.group(activity).readEntry("mode", "None") == "ActLike") {
            continue;
        }

        KActivityInfo *info = new KActivityInfo(activity, this);
        QString icon = info->icon();
        QString name = i18nc("This is meant to be: Act like activity %1",
                             "Activity \"%1\"", info->name());

        m_ui->actLikeComboBox->addItem(KIcon(icon), name, activity);
    }

    // Proper loading routine

    if (config.readEntry("mode", QString()) == "ActLike") {
        m_ui->actLikeRadio->setChecked(true);
        m_ui->actLikeComboBox->setCurrentIndex(m_ui->actLikeComboBox->findData(config.readEntry("actLike", QString())));
    } else if (config.readEntry("mode", QString()) == "SpecialBehavior") {
        m_ui->specialBehaviorRadio->setChecked(true);
        KConfigGroup behaviorGroup = config.group("SpecialBehavior");

        m_ui->noShutdownPCBox->setChecked(behaviorGroup.readEntry("noSuspend", false));
        m_ui->noShutdownScreenBox->setChecked(behaviorGroup.readEntry("noScreenManagement", false));
        m_ui->alwaysBox->setChecked(behaviorGroup.readEntry("performAction", false));

        KConfigGroup actionConfig = behaviorGroup.group("ActionConfig");
        m_ui->alwaysActionBox->setCurrentIndex(m_ui->alwaysActionBox->findData(actionConfig.readEntry("suspendType", 0)));
        m_ui->alwaysAfterSpin->setValue(actionConfig.readEntry("idleTime", 600000) / 60 / 1000);
    } else if (config.readEntry("mode", QString()) == "SeparateSettings") {
        m_ui->separateSettingsRadio->setChecked(true);

        m_actionEditWidget->load();
    }
}

void ActivityWidget::save()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    if (m_ui->actLikeRadio->isChecked()) {
        config.writeEntry("mode", "ActLike");
        config.writeEntry("actLike", m_ui->actLikeComboBox->itemData(m_ui->actLikeComboBox->currentIndex()).toString());
    } else if (m_ui->specialBehaviorRadio->isChecked()) {
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
    } else if (m_ui->separateSettingsRadio->isChecked()) {
        config.writeEntry("mode", "SeparateSettings");
        m_actionEditWidget->save();
    } else {
        config.writeEntry("mode", "None");
    }

    config.sync();
}

void ActivityWidget::setChanged()
{
    emit changed(true);
}


#include "activitywidget.moc"
