/*
 *   SPDX-FileCopyrightText: 2008-2011 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

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
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDebug>

ActionEditWidget::ActionEditWidget(const QString &configName, QWidget *parent)
    : QWidget(parent)
    , m_configName(configName)
    , m_profilesConfig(KSharedConfig::openConfig(QStringLiteral("powermanagementprofilesrc"), KConfig::SimpleConfig | KConfig::CascadeConfig))
{
    ActionConfigWidget *actionConfigWidget = new ActionConfigWidget(nullptr);
    QMultiMap<int, QList<QPair<QString, QWidget *>>> widgets;

    // Load all the plugins
    const QVector<KPluginMetaData> offers = KPluginMetaData::findPlugins(QStringLiteral("powerdevil/action"));

    for (const KPluginMetaData &offer : offers) {
        // Does it have a runtime requirement?
        if (offer.value(QStringLiteral("X-KDE-PowerDevil-Action-HasRuntimeRequirement"), false)) {
            qCDebug(POWERDEVIL) << offer.name() << " has a runtime requirement";

            QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement",
                                                               "/org/kde/Solid/PowerManagement",
                                                               "org.kde.Solid.PowerManagement",
                                                               "isActionSupported");
            call.setArguments(QVariantList() << offer.value("X-KDE-PowerDevil-Action-ID"));
            QDBusPendingReply<bool> reply = QDBusConnection::sessionBus().asyncCall(call);
            reply.waitForFinished();

            if (reply.isValid()) {
                if (!reply.value()) {
                    qCDebug(POWERDEVIL) << "The action " << offer.value("X-KDE-PowerDevil-Action-ID") << " appears not to be supported by the core.";
                    continue;
                }
            } else {
                qCDebug(POWERDEVIL) << "There was a problem in contacting DBus!! Assuming the action is ok.";
            }
        }

        PowerDevil::ActionConfig *actionConfig = nullptr;
        // try to load the library derived from plugin id
        KPluginMetaData uiLib(QLatin1String("powerdevil/action/kcm/") + offer.pluginId() + QLatin1String("_config"), KPluginMetaData::AllowEmptyMetaData);
        if (uiLib.isValid()) {
            actionConfig = KPluginFactory::instantiatePlugin<PowerDevil::ActionConfig>(uiLib).plugin;
        }
        if (!actionConfig) {
            continue;
        }

        connect(actionConfig, &PowerDevil::ActionConfig::changed, this, &ActionEditWidget::onChanged);

        QCheckBox *checkbox = new QCheckBox(offer.name());
        connect(checkbox, &QCheckBox::stateChanged, this, &ActionEditWidget::onChanged);
        m_actionsHash.insert(offer.value(QStringLiteral("X-KDE-PowerDevil-Action-ID")), checkbox);
        m_actionsConfigHash.insert(offer.value(QStringLiteral("X-KDE-PowerDevil-Action-ID")), actionConfig);

        QList<QPair<QString, QWidget *>> offerWidgets = actionConfig->buildUi();
        offerWidgets.prepend(qMakePair<QString, QWidget *>(QString(), checkbox));
        widgets.insert(100 - offer.value(QStringLiteral("X-KDE-PowerDevil-Action-ConfigPriority"), 0), offerWidgets);
    }

    for (QMultiMap<int, QList<QPair<QString, QWidget *>>>::const_iterator i = widgets.constBegin(); i != widgets.constEnd(); ++i) {
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
    for (QHash<QString, QCheckBox *>::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
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
    for (QHash<QString, QCheckBox *>::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
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

#include "moc_actioneditwidget.cpp"
