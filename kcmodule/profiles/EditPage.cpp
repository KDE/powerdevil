/***************************************************************************
 *   Copyright (C) 2008-2010 by Dario Freddi <drf@kde.org>                 *
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

#include "EditPage.h"

#include "actioneditwidget.h"
#include "ErrorOverlay.h"

#include <powerdevilactionconfig.h>
#include <powerdevilprofilegenerator.h>
#include <powerdevilpowermanagement.h>

#include <powerdevil_debug.h>
#include <Kirigami/TabletModeWatcher>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QTabBar>

#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>

#include <KConfigGroup>
#include <QDebug>
#include <KMessageBox>
#include <KAboutData>
#include <KPluginFactory>
#include <KLocalizedString>

#include <Solid/Battery>
#include <Solid/Device>

K_PLUGIN_CLASS_WITH_JSON(EditPage, "kcm_powerdevilprofilesconfig.json")

EditPage::EditPage(QObject *parent, const KPluginMetaData&data, const QVariantList &args)
    : KCModule(parent, data, args)
{
    setButtons(Apply | Help | Default);

    setupUi(widget());

    m_profilesConfig = KSharedConfig::openConfig("powermanagementprofilesrc", KConfig::SimpleConfig | KConfig::CascadeConfig);

    if (m_profilesConfig->groupList().isEmpty()) {
        auto interface = Kirigami::TabletModeWatcher::self();

        PowerDevil::ProfileGenerator::generateProfiles(
            interface->isTabletMode(),
            PowerDevil::PowerManagement::instance()->canSuspend(),
            PowerDevil::PowerManagement::instance()->canHibernate()
        );
        m_profilesConfig->reparseConfiguration();
    }

    qCDebug(POWERDEVIL) << "loaded profiles" << m_profilesConfig.data()->groupList() << m_profilesConfig.data()->entryMap().keys();

    // Create widgets for each profile
    ActionEditWidget *editWidget = new ActionEditWidget("AC", tabWidget);
    m_editWidgets.insert("AC", editWidget);
    acWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    editWidget = new ActionEditWidget("Battery", tabWidget);
    m_editWidgets.insert("Battery", editWidget);
    batteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    editWidget = new ActionEditWidget("LowBattery", tabWidget);
    m_editWidgets.insert("LowBattery", editWidget);
    lowBatteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &EditPage::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &EditPage::onServiceUnregistered);

    bool hasBattery = false;
    const auto batteries = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for(const Solid::Device &device : batteries) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply() && (b->type() == Solid::Battery::PrimaryBattery || b->type() == Solid::Battery::UpsBattery)) {
            hasBattery = true;
            break;
        }
    }

    if (!hasBattery) {
        tabWidget->setTabEnabled(1, false);
        tabWidget->setTabEnabled(2, false);
        tabWidget->tabBar()->hide();
    }

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }
}

void EditPage::onChanged(bool value)
{
    ActionEditWidget *editWidget = qobject_cast< ActionEditWidget* >(sender());
    if (!editWidget) {
        return;
    }

    m_profileEdited[editWidget->configName()] = value;

    if (value) {
        setNeedsSave(true);
    }

    checkAndEmitChanged();
}

void EditPage::load()
{
    qCDebug(POWERDEVIL) << "Loading routine called";
    for (QHash< QString, ActionEditWidget* >::const_iterator i = m_editWidgets.constBegin();
         i != m_editWidgets.constEnd(); ++i) {
        i.value()->load();

        m_profileEdited[i.value()->configName()] = false;
    }
}

void EditPage::save()
{
    for (auto it = m_editWidgets.constBegin(); it != m_editWidgets.constEnd(); ++it) {
        (*it)->save();
    }

    notifyDaemon();

    setNeedsSave(false);
}

void EditPage::notifyDaemon()
{
    QDBusConnection::sessionBus().asyncCall(
        QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("refreshStatus")
        )
    );
}

void EditPage::restoreDefaultProfiles()
{
    // Confirm
    int ret = KMessageBox::warningContinueCancel(widget(),
                                                 i18n("The KDE Power Management System will now generate a set of defaults "
                                                      "based on your computer's capabilities. This will also erase "
                                                      "all existing modifications you made. "
                                                      "Are you sure you want to continue?"),
                                                 i18n("Restore Default Profiles"));
    if (ret == KMessageBox::Continue) {
        qCDebug(POWERDEVIL) << "Restoring defaults.";
        auto interface = Kirigami::TabletModeWatcher::self();

        PowerDevil::ProfileGenerator::generateProfiles(
            interface->isTabletMode(),
            PowerDevil::PowerManagement::instance()->canSuspend(),
            PowerDevil::PowerManagement::instance()->canHibernate()
        );

        load();

        notifyDaemon();
    }
}

void EditPage::defaults()
{
    restoreDefaultProfiles();
}

void EditPage::checkAndEmitChanged()
{
    bool value = false;
    for (QHash< QString, bool >::const_iterator i = m_profileEdited.constBegin();
         i != m_profileEdited.constEnd(); ++i) {
        if (i.value()) {
            value = i.value();
        }
    }

    setNeedsSave(value);
}

void EditPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

    QDBusPendingCallWatcher *currentProfileWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(
        QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("currentProfile")
        )
    ), this);

    QObject::connect(currentProfileWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QString> reply = *watcher;

        if (!reply.isError()) {
            const QString &currentProfile = reply.value();
            if (currentProfile == QLatin1String("Battery")) {
                tabWidget->setCurrentIndex(1);
            } else if (currentProfile == QLatin1String("LowBattery")) {
                tabWidget->setCurrentIndex(2);
            }
        }

        watcher->deleteLater();
    });

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
        m_errorOverlay = nullptr;
    }
}

void EditPage::onServiceUnregistered(const QString& service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
    }

    m_errorOverlay = new ErrorOverlay(widget(), i18n("The Power Management Service appears not to be running."), widget());
}

#include "EditPage.moc"
