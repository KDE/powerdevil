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

#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDebug>

ActionEditWidget::ActionEditWidget(const QString &configName, std::unique_ptr<PowerDevil::ProfileSettings> profileSettings, QWidget *parent)
    : QWidget(parent)
    , m_configName(configName)
    , m_profileSettings(std::move(profileSettings))
{
    ActionConfigWidget *actionConfigWidget = new ActionConfigWidget(nullptr);
    QMultiMap<int, QList<QPair<QString, QWidget *>>> widgets;

    // Load all the plugins
    const QList<KPluginMetaData> offers = KPluginMetaData::findPlugins(QStringLiteral("powerdevil/action"));

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
            actionConfig->setProfileSettings(m_profileSettings.get());
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
    m_profileSettings->load();

    qCDebug(POWERDEVIL) << "PowerDevil::ProfileSettings ready:" << m_configName;
    for (KConfigSkeletonItem *item : m_profileSettings->items()) {
        qCDebug(POWERDEVIL) << item->key() << "=" << item->property();
    }

    // Iterate over the possible actions
    for (QHash<QString, QCheckBox *>::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        i.value()->setChecked(m_actionsConfigHash[i.key()]->enabledInProfileSettings());
        m_actionsConfigHash[i.key()]->load();
    }

    Q_EMIT changed(false);
}

void ActionEditWidget::save()
{
    // Iterate over the possible actions
    for (QHash<QString, QCheckBox *>::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        m_actionsConfigHash[i.key()]->setEnabledInProfileSettings(i.value()->isChecked());
        m_actionsConfigHash[i.key()]->save();
    }

    m_profileSettings->save();
    // Reloading settings should not be required here given that it's backed by a KSharedConfig,
    // and we're only interested in the single profile that this ActionEditWidget was initialized with.

    Q_EMIT changed(false);
}

void ActionEditWidget::setDefaults()
{
    m_profileSettings->setDefaults();

    // Iterate over the possible actions
    for (QHash<QString, QCheckBox *>::const_iterator i = m_actionsHash.constBegin(); i != m_actionsHash.constEnd(); ++i) {
        i.value()->setChecked(m_actionsConfigHash[i.key()]->enabledInProfileSettings());
        m_actionsConfigHash[i.key()]->load();
    }
}

void ActionEditWidget::onChanged()
{
    Q_EMIT changed(true);
}

QString ActionEditWidget::configName() const
{
    return m_configName;
}

#include "moc_actioneditwidget.cpp"
