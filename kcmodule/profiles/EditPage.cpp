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

#include <powerdevilprofilegenerator.h>
#include <powerdevilpowermanagement.h>

#include <powerdevil_debug.h>
#include <TabletModeWatcher>

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
#include <QTabWidget>
#include <QHBoxLayout>

#include <KConfigGroup>
#include <QDebug>
#include <KMessageBox>
#include <KAboutData>
#include <KPluginFactory>
#include <KLocalizedString>
#include <KLocalizedContext>

#include <KRun>

#include <Solid/Battery>
#include <Solid/Device>

K_PLUGIN_FACTORY(PowerDevilProfilesKCMFactory,
                 registerPlugin<EditPage>();
                )

EditPage::EditPage(QWidget *parent, const QVariantList &args)
        : KCModule(parent, args)
{
    setButtons(Apply | Help | Default);

//     KAboutData *about =
//         new KAboutData("powerdevilprofilesconfig", "powerdevilprofilesconfig", ki18n("Power Profiles Configuration"),
//                        "", ki18n("A profile configurator for KDE Power Management System"),
//                        KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
//                        ki18n("From this module, you can manage KDE Power Management System's power profiles, by tweaking "
//                              "existing ones or creating new ones."));
//
//     about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
//                      "http://drfav.wordpress.com");
//
//     setAboutData(about);

    tabWidget = new QTabWidget();
    auto hLayout = new QHBoxLayout();
    setLayout(hLayout);

    // Create widgets for each profile
    m_ACConfig.reset(new PowerDevilProfileSettings(QStringLiteral("AC")));

    m_BatteryConfig.reset(new PowerDevilProfileSettings(QStringLiteral("Battery")));

    m_LowBatteryConfig.reset(new PowerDevilProfileSettings(QStringLiteral("LowBattery")));


    auto *acEditWidget = new ActionEditWidget("AC");
    tabWidget->addTab(acEditWidget, i18nc("@tab: plugged into wired energy", "AC"));
    m_editWidgets.append(acEditWidget);
    addConfig(m_ACConfig.get(), acEditWidget);

    auto *batteryEditWidget = new ActionEditWidget("Battery", tabWidget);
    tabWidget->addTab(batteryEditWidget, i18nc("@tab: On battery", "Battery"));
    m_editWidgets.append(batteryEditWidget);
    addConfig(m_BatteryConfig.get(), batteryEditWidget);

    auto *lowBatteryEditWidget = new ActionEditWidget("LowBattery", tabWidget);
    tabWidget->addTab(lowBatteryEditWidget, i18nc("@tab: on low battery", "Low Battery"));
    m_editWidgets.append(lowBatteryEditWidget);
    addConfig(m_LowBatteryConfig.get(), lowBatteryEditWidget);

    hLayout->addWidget(tabWidget);

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

    for (auto *wdg : {acEditWidget, batteryEditWidget, lowBatteryEditWidget}) {
        connect(wdg, &ActionEditWidget::requestDefaultState, this, [this] {
            if (m_ACConfig->isDefaults() && m_BatteryConfig->isDefaults() && m_LowBatteryConfig->isDefaults()) {
                unmanagedWidgetDefaultState(true);
            }
        });
        connect(wdg, &ActionEditWidget::requestChangeState, this, [this] {
            unmanagedWidgetChangeState(true);
        });
    }
}

void EditPage::forEachTab(std::function<void(ActionEditWidget*)> func)
{
    for (int i = 0; i < tabWidget->count(); i++) {
        if (tabWidget->isTabEnabled(i)) {
            auto actionWidgegt = qobject_cast<ActionEditWidget*>(tabWidget->widget(i));
            if (actionWidgegt) {
                func(actionWidgegt);
            }
        }
    }
}

void EditPage::load()
{
    KCModule::load();
    forEachTab([](ActionEditWidget* widget) { widget->load(); });
}

void EditPage::save()
{
    notifyDaemon();
    forEachTab([](ActionEditWidget* widget) { widget->save(); });
    KCModule::save();
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

void EditPage::openUrl(const QString &url)
{
    new KRun(QUrl(url), this);
}

void EditPage::defaults()
{
    KCModule::defaults();
    notifyDaemon();
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

    m_errorOverlay = new ErrorOverlay(this, i18n("The Power Management Service appears not to be running."),
                                      this);
}

#include "EditPage.moc"
