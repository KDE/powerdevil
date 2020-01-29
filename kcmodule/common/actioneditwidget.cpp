/***************************************************************************
 *   Copyright (C) 2008-2011 by Dario Freddi <drf@kde.org>                 *
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


#include "actioneditwidget.h"

#include "actionconfigwidget.h"

#include <powerdevilaction.h>
#include <powerdevilactionconfig.h>

#include <powerdevil_debug.h>

#include <QCheckBox>
#include <QVBoxLayout>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>

#include <KConfigGroup>
#include <QDebug>
#include <KServiceTypeTrader>

ActionEditWidget::ActionEditWidget(const QString &configName, QWidget *parent)
    : QWidget(parent)
    , m_configName(configName)
{
    m_profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig | KConfig::CascadeConfig);

    ActionConfigWidget *actionConfigWidget = new ActionConfigWidget(nullptr);
    QMultiMap< int, QList<QPair<QString, QWidget*> > > widgets;

    // Load all the services
    const KService::List offers = KServiceTypeTrader::self()->query("PowerDevil/Action", "(Type == 'Service')");

    for (const KService::Ptr &offer : offers) {
        // Does it have a runtime requirement?
        if (offer->property("X-KDE-PowerDevil-Action-HasRuntimeRequirement", QVariant::Bool).toBool()) {
            qCDebug(POWERDEVIL) << offer->name() << " has a runtime requirement";

            QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                               "org.kde.Solid.PowerManagement", "isActionSupported");
            call.setArguments(QVariantList() << offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String));
            QDBusPendingReply< bool > reply = QDBusConnection::sessionBus().asyncCall(call);
            reply.waitForFinished();

            if (reply.isValid()) {
                if (!reply.value()) {
                    qCDebug(POWERDEVIL) << "The action " << offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String) << " appears not to be supported by the core.";
                    continue;
                }
            } else {
                qCDebug(POWERDEVIL) << "There was a problem in contacting DBus!! Assuming the action is ok.";
            }
        }

        //try to load the specified library
        KPluginFactory *factory = KPluginLoader(offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                QVariant::String).toString()).factory();

        if (!factory) {
            qCWarning(POWERDEVIL) << "KPluginFactory could not load the plugin:" << offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                       QVariant::String).toString();
            continue;
        }

        PowerDevil::ActionConfig *actionConfig = factory->create<PowerDevil::ActionConfig>();
        if (!actionConfig) {
            qCWarning(POWERDEVIL) << "KPluginFactory could not load the plugin:" << offer->property("X-KDE-PowerDevil-Action-UIComponentLibrary",
                                                                       QVariant::String).toString();
            continue;
        }

        connect(actionConfig, SIGNAL(changed()), this, SLOT(onChanged()));

        QCheckBox *checkbox = new QCheckBox(offer->name());
        connect(checkbox, SIGNAL(stateChanged(int)), this, SLOT(onChanged()));
        m_actionsHash.insert(offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString(), checkbox);
        m_actionsConfigHash.insert(offer->property("X-KDE-PowerDevil-Action-ID", QVariant::String).toString(), actionConfig);

        QList<QPair<QString, QWidget*> > offerWidgets = actionConfig->buildUi();
        offerWidgets.prepend(qMakePair<QString,QWidget*>(QString(), checkbox));
        widgets.insert(100 - offer->property("X-KDE-PowerDevil-Action-ConfigPriority", QVariant::Int).toInt(),
                            offerWidgets);
    }

    for (QMultiMap< int, QList<QPair<QString, QWidget*> > >::const_iterator i = widgets.constBegin(); i != widgets.constEnd(); ++i) {
        actionConfigWidget->addWidgets(i.value());
    }

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(actionConfigWidget);
    lay->addStretch();
    setLayout(lay);
}

ActionEditWidget::~ActionEditWidget()
{

}

void ActionEditWidget::load()
{
    KConfigGroup group = configGroup();

    qCDebug(POWERDEVIL) << m_profilesConfig.data()->entryMap().keys();

    if (!group.isValid()) {
        return;
    }
    qCDebug(POWERDEVIL) << "Ok, KConfigGroup ready" << group.entryMap().keys();

    // Iterate over the possible actions
    for (QHash< QString, QCheckBox* >::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        i.value()->setChecked(group.groupList().contains(i.key()));

        KConfigGroup actionGroup = group.group(i.key());
        m_actionsConfigHash[i.key()]->setConfigGroup(actionGroup);
        m_actionsConfigHash[i.key()]->load();
    }

    Q_EMIT changed(false);
}

void ActionEditWidget::save()
{
    KConfigGroup group = configGroup();

    if (!group.isValid()) {
        qCDebug(POWERDEVIL) << "Could not perform a save operation, group is not valid!";
        return;
    }

    // Iterate over the possible actions
    for (QHash< QString, QCheckBox* >::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        if (i.value()->isChecked()) {
            // Perform the actual save
            m_actionsConfigHash[i.key()]->save();
        } else {
            // Erase the group
            group.deleteGroup(i.key());
        }
    }

    group.sync();

    // After saving, reload the config to make sure we'll pick up changes.
    m_profilesConfig.data()->reparseConfiguration();

    Q_EMIT changed(false);
}

void ActionEditWidget::onChanged()
{
    Q_EMIT changed(true);
}

QString ActionEditWidget::configName() const
{
    return m_configName;
}

KConfigGroup ActionEditWidget::configGroup()
{
    if (!m_configName.contains('/')) {
        return KConfigGroup(m_profilesConfig, m_configName);
    } else {
        QStringList names = m_configName.split('/');
        KConfigGroup retgroup(m_profilesConfig, names.first());

        QStringList::const_iterator i = names.constBegin();
        ++i;

        while (i != names.constEnd()) {
            retgroup = retgroup.group(*i);
            ++i;
        }

        return retgroup;
    }
}
