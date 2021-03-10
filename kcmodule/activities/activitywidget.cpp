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

#include <KConfigGroup>
#include <KActivities/Consumer>
#include <Solid/Battery>
#include <Solid/Device>
#include <actioneditwidget.h>
#include <QLayout>

ActivityWidget::ActivityWidget(const QString& activity, QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ActivityWidget)
    , m_profilesConfig(KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig | KConfig::CascadeConfig))
    , m_activity(activity)
    , m_activityConsumer(new KActivities::Consumer(this))
    , m_actionEditWidget(new ActionEditWidget(QString("Activities/%1/SeparateSettings").arg(activity)))
{
    m_ui->setupUi(this);

    m_ui->separateSettingsLayout->addWidget(m_actionEditWidget);

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

    m_actionEditWidget->setVisible(false);
    m_actionEditWidget->load();

    connect(m_ui->separateSettingsRadio, &QAbstractButton::toggled, m_actionEditWidget, &QWidget::setVisible);

    connect(m_ui->actLikeRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->noSettingsRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->separateSettingsRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->specialBehaviorRadio, &QAbstractButton::toggled, this, &ActivityWidget::setChanged);
    connect(m_ui->actLikeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_ui->alwaysActionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setChanged()));
    connect(m_ui->alwaysAfterSpin, SIGNAL(valueChanged(int)), this, SLOT(setChanged()));

    connect(m_actionEditWidget, &ActionEditWidget::changed, this, &ActivityWidget::changed);
}

ActivityWidget::~ActivityWidget()
{

}

void ActivityWidget::load()
{
    KConfigGroup activitiesGroup(m_profilesConfig, "Activities");
    KConfigGroup config = activitiesGroup.group(m_activity);

    using namespace PowerDevil::BundledActions;

    if (PowerDevil::PowerManagement::instance()->canSuspend()) {
        m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-suspend"),
                                       i18nc("Suspend to RAM", "Sleep"), (uint)SuspendSession::ToRamMode);
    }
    if (PowerDevil::PowerManagement::instance()->canHibernate()) {
        m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-suspend-hibernate"),
                                       i18n("Hibernate"), (uint)SuspendSession::ToDiskMode);
    }
    m_ui->alwaysActionBox->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), (uint)SuspendSession::ShutdownMode);

    m_ui->actLikeComboBox->clear();

    m_ui->actLikeComboBox->addItem(QIcon::fromTheme("battery-charging"), i18n("PC running on AC power"), "AC");
    m_ui->actLikeComboBox->addItem(QIcon::fromTheme("battery-060"), i18n("PC running on battery power"), "Battery");
    m_ui->actLikeComboBox->addItem(QIcon::fromTheme("battery-low"), i18n("PC running on low battery"), "LowBattery");

    bool hasBattery = false;
    const auto batteries = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for (const Solid::Device &device : batteries) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery) {
            hasBattery = false;
            break;
        }
    }

    m_ui->actLikeRadio->setVisible(hasBattery);
    m_ui->actLikeComboBox->setVisible(hasBattery);

    const QStringList activities = m_activityConsumer->activities();
    for (const QString &activity : activities) {
        if (activity == m_activity) {
            continue;
        }

        if (activitiesGroup.group(activity).readEntry("mode", "None") == "None" ||
            activitiesGroup.group(activity).readEntry("mode", "None") == "ActLike") {
            continue;
        }

        KActivities::Info *info = new KActivities::Info(activity, this);
        QString icon = info->icon();
        QString name = i18nc("This is meant to be: Act like activity %1",
                             "Activity \"%1\"", info->name());

        m_ui->actLikeComboBox->addItem(QIcon::fromTheme(icon), name, activity);
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
    Q_EMIT changed(true);
}
