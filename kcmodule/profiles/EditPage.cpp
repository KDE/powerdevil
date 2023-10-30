/*
 *   SPDX-FileCopyrightText: 2008-2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "EditPage.h"

#include "ErrorOverlay.h"
#include "actioneditwidget.h"

#include <powerdevilactionconfig.h>
#include <powerdevilpowermanagement.h>

#include <PowerDevilProfileSettings.h>
#include <powerdevil_debug.h>

#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QTabBar>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusServiceWatcher>

#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>
#include <Kirigami/Platform/TabletModeWatcher>

#include <Solid/Battery>
#include <Solid/Device>

K_PLUGIN_CLASS_WITH_JSON(EditPage, "kcm_powerdevilprofilesconfig.json")

EditPage::EditPage(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    setButtons(Apply | Help | Default);

    setupUi(widget());

    bool isMobile = Kirigami::Platform::TabletModeWatcher::self()->isTabletMode(); // FIXME: problematic, see powerdevil GitLab issue #16
    bool isVM = PowerDevil::PowerManagement::instance()->isVirtualMachine();
    bool canSuspend = PowerDevil::PowerManagement::instance()->canSuspend();

    // Create config and widgets for each profile
    auto profileSettings = std::make_unique<PowerDevil::ProfileSettings>("AC", isMobile, isVM, canSuspend);
    ActionEditWidget *editWidget = new ActionEditWidget("AC", std::move(profileSettings), tabWidget);
    m_editWidgets.insert("AC", editWidget);
    acWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    profileSettings = std::make_unique<PowerDevil::ProfileSettings>("Battery", isMobile, isVM, canSuspend);
    editWidget = new ActionEditWidget("Battery", std::move(profileSettings), tabWidget);
    m_editWidgets.insert("Battery", editWidget);
    batteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    profileSettings = std::make_unique<PowerDevil::ProfileSettings>("LowBattery", isMobile, isVM, canSuspend);
    editWidget = new ActionEditWidget("LowBattery", std::move(profileSettings), tabWidget);
    m_editWidgets.insert("LowBattery", editWidget);
    lowBatteryWidgetLayout->addWidget(editWidget);
    connect(editWidget, &ActionEditWidget::changed, this, &EditPage::onChanged);

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &EditPage::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &EditPage::onServiceUnregistered);

    bool hasBattery = false;
    const auto batteries = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for (const Solid::Device &device : batteries) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery *>(device.asDeviceInterface(Solid::DeviceInterface::Battery));
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
    ActionEditWidget *editWidget = qobject_cast<ActionEditWidget *>(sender());
    if (!editWidget) {
        return;
    }

    m_profileEdited[editWidget->configName()] = value;
    checkAndEmitChanged();
}

void EditPage::load()
{
    qCDebug(POWERDEVIL) << "Loading routine called";
    for (QHash<QString, ActionEditWidget *>::const_iterator i = m_editWidgets.constBegin(); i != m_editWidgets.constEnd(); ++i) {
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
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                                           QStringLiteral("/org/kde/Solid/PowerManagement"),
                                                                           QStringLiteral("org.kde.Solid.PowerManagement"),
                                                                           QStringLiteral("refreshStatus")));
}

void EditPage::defaults()
{
    for (auto it = m_editWidgets.constBegin(); it != m_editWidgets.constEnd(); ++it) {
        (*it)->setDefaults();
    }
}

void EditPage::checkAndEmitChanged()
{
    bool value = false;
    for (QHash<QString, bool>::const_iterator i = m_profileEdited.constBegin(); i != m_profileEdited.constEnd(); ++i) {
        if (i.value()) {
            value |= i.value();
        }
    }

    setNeedsSave(value);
}

void EditPage::onServiceRegistered(const QString &service)
{
    Q_UNUSED(service);

    QDBusPendingCallWatcher *currentProfileWatcher =
        new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                                                                           QStringLiteral("/org/kde/Solid/PowerManagement"),
                                                                                                           QStringLiteral("org.kde.Solid.PowerManagement"),
                                                                                                           QStringLiteral("currentProfile"))),
                                    this);

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

void EditPage::onServiceUnregistered(const QString &service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
    }

    m_errorOverlay = new ErrorOverlay(widget(), i18n("The Power Management Service appears not to be running."), widget());
}

#include "EditPage.moc"

#include "moc_EditPage.cpp"
