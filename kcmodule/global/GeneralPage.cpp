/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kde.org>                      *
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

#include "GeneralPage.h"

#include "ErrorOverlay.h"
#include "PowerDevilSettings.h"

#include "actions/bundled/suspendsession.h"

#include "powerdevilpowermanagement.h"

#include <Solid/Device>
#include <Solid/DeviceInterface>
#include <Solid/Battery>

#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusServiceWatcher>

#include <KNotifyConfigWidget>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KAboutData>
#include <KLocalizedString>

#include <KAuthAction>
#include <KAuthExecuteJob>

K_PLUGIN_FACTORY(PowerDevilGeneralKCMFactory,
                 registerPlugin<GeneralPage>();
                )

GeneralPage::GeneralPage(QWidget *parent, const QVariantList &args)
        : KCModule(nullptr, parent, args)
{
    setButtons(Apply | Help);

//     KAboutData *about =
//         new KAboutData("powerdevilglobalconfig", "powerdevilglobalconfig", ki18n("Global Power Management Configuration"),
//                        "", ki18n("A global power management configurator for KDE Power Management System"),
//                        KAboutData::License_GPL, ki18n("(c), 2010 Dario Freddi"),
//                        ki18n("From this module, you can configure the main Power Management daemon, assign profiles to "
//                              "states, and do some advanced fine tuning on battery handling"));
//
//     about->addAuthor(ki18n("Dario Freddi"), ki18n("Maintainer") , "drf@kde.org",
//                      "http://drfav.wordpress.com");
//
//     setAboutData(about);

    setupUi(this);

    fillUi();

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher("org.kde.Solid.PowerManagement",
                                                           QDBusConnection::sessionBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration,
                                                           this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &GeneralPage::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &GeneralPage::onServiceUnregistered);

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.Solid.PowerManagement")) {
        onServiceRegistered("org.kde.Solid.PowerManagement");
    } else {
        onServiceUnregistered("org.kde.Solid.PowerManagement");
    }
}

GeneralPage::~GeneralPage()
{
}

void GeneralPage::fillUi()
{
    bool hasPowerSupplyBattery = false;
    bool hasPeripheralBattery = false;

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
    for (const Solid::Device &device : devices) {
        const Solid::Battery *b = qobject_cast<const Solid::Battery*> (device.asDeviceInterface(Solid::DeviceInterface::Battery));
        if (b->isPowerSupply()) {
            hasPowerSupplyBattery = true;
        } else {
            hasPeripheralBattery = true;
        }
    }

    BatteryCriticalCombo->addItem(QIcon::fromTheme("dialog-cancel"), i18n("Do nothing"), PowerDevil::BundledActions::SuspendSession::None);
    if (PowerDevil::PowerManagement::instance()->canSuspend()) {
        BatteryCriticalCombo->addItem(QIcon::fromTheme("system-suspend"), i18nc("Suspend to RAM", "Sleep"), PowerDevil::BundledActions::SuspendSession::ToRamMode);
    }
    if (PowerDevil::PowerManagement::instance()->canHibernate()) {
        BatteryCriticalCombo->addItem(QIcon::fromTheme("system-suspend-hibernate"), i18n("Hibernate"), PowerDevil::BundledActions::SuspendSession::ToDiskMode);
    }
    BatteryCriticalCombo->addItem(QIcon::fromTheme("system-shutdown"), i18n("Shut down"), PowerDevil::BundledActions::SuspendSession::ShutdownMode);

    notificationsButton->setIcon(QIcon::fromTheme("preferences-desktop-notification"));

    // modified fields...

    connect(notificationsButton, &QAbstractButton::clicked, this, &GeneralPage::configureNotifications);

    connect(lowSpin, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(criticalSpin, SIGNAL(valueChanged(int)), SLOT(changed()));
    connect(lowPeripheralSpin, SIGNAL(valueChanged(int)), SLOT(changed()));

    connect(BatteryCriticalCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));

    connect(chargeStartThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &GeneralPage::markAsChanged);
    connect(chargeStopThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &GeneralPage::onChargeStopThresholdChanged);
    chargeStopThresholdMessage->hide();

    connect(pausePlayersCheckBox, SIGNAL(stateChanged(int)), SLOT(changed()));

    if (!hasPowerSupplyBattery) {
        BatteryCriticalLabel->hide();
        BatteryCriticalCombo->hide();
        lowLabel->hide();
        lowSpin->hide();
        criticalLabel->hide();
        criticalSpin->hide();
    }

    if (!hasPeripheralBattery) {
        lowPeripheralLabel->hide();
        lowPeripheralSpin->hide();
    }

    if (!hasPowerSupplyBattery && !hasPeripheralBattery) {
        batteryLevelsLabel->hide();
    }
}

void GeneralPage::load()
{
    lowSpin->setValue(PowerDevilSettings::batteryLowLevel());
    criticalSpin->setValue(PowerDevilSettings::batteryCriticalLevel());
    lowPeripheralSpin->setValue(PowerDevilSettings::peripheralBatteryLowLevel());

    BatteryCriticalCombo->setCurrentIndex(BatteryCriticalCombo->findData(PowerDevilSettings::batteryCriticalAction()));

    pausePlayersCheckBox->setChecked(PowerDevilSettings::pausePlayersOnSuspend());

    KAuth::Action action(QStringLiteral("org.kde.powerdevil.chargethresholdhelper.getthreshold"));
    action.setHelperId(QStringLiteral("org.kde.powerdevil.chargethresholdhelper"));
    KAuth::ExecuteJob *job = action.execute();
    job->exec();

    if (!job->error()) {
        const auto data = job->data();
        m_chargeStartThreshold = data.value(QStringLiteral("chargeStartThreshold")).toInt();
        chargeStartThresholdSpin->setValue(m_chargeStartThreshold);
        m_chargeStopThreshold = data.value(QStringLiteral("chargeStopThreshold")).toInt();
        chargeStopThresholdSpin->setValue(m_chargeStopThreshold);

        setChargeThresholdSupported(true);
    } else {
        qDebug() << "org.kde.powerdevil.chargethresholdhelper.getthreshold failed" << job->errorText();
        setChargeThresholdSupported(false);
    }

    Q_EMIT changed(false);
}

void GeneralPage::configureNotifications()
{
    KNotifyConfigWidget::configure(this, "powerdevil");
}

void GeneralPage::save()
{
    PowerDevilSettings::setBatteryLowLevel(lowSpin->value());
    PowerDevilSettings::setBatteryCriticalLevel(criticalSpin->value());
    PowerDevilSettings::setPeripheralBatteryLowLevel(lowPeripheralSpin->value());

    PowerDevilSettings::setBatteryCriticalAction(BatteryCriticalCombo->itemData(BatteryCriticalCombo->currentIndex()).toInt());

    PowerDevilSettings::setPausePlayersOnSuspend(pausePlayersCheckBox->checkState() == Qt::Checked);

    PowerDevilSettings::self()->save();

    if (chargeStartThresholdSpin->value() != m_chargeStartThreshold
            || chargeStopThresholdSpin->value() != m_chargeStopThreshold) {
        KAuth::Action action(QStringLiteral("org.kde.powerdevil.chargethresholdhelper.setthreshold"));
        action.setHelperId(QStringLiteral("org.kde.powerdevil.chargethresholdhelper"));
        action.setArguments({
            {QStringLiteral("chargeStartThreshold"), chargeStartThresholdSpin->value()},
            {QStringLiteral("chargeStopThreshold"), chargeStopThresholdSpin->value()}
        });
        KAuth::ExecuteJob *job = action.execute();
        job->exec();
        if (!job->error()) {
            m_chargeStartThreshold = chargeStartThresholdSpin->value();
            m_chargeStopThreshold = chargeStopThresholdSpin->value();
        }
    }

    // Notify Daemon
    QDBusMessage call = QDBusMessage::createMethodCall("org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement",
                                                       "org.kde.Solid.PowerManagement", "refreshStatus");

    // Perform call
    QDBusConnection::sessionBus().asyncCall(call);

    // And now we are set with no change
    Q_EMIT changed(false);
}

void GeneralPage::defaults()
{
    KCModule::defaults();
}

void GeneralPage::setChargeThresholdSupported(bool supported)
{
    batteryThresholdLabel->setVisible(supported);
    batteryThresholdExplanation->setVisible(supported);

    chargeStartThresholdLabel->setVisible(supported);
    chargeStartThresholdSpin->setVisible(supported);
    chargeStopThresholdLabel->setVisible(supported);
    chargeStopThresholdSpin->setVisible(supported);
}

void GeneralPage::onServiceRegistered(const QString& service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
        m_errorOverlay = nullptr;
    }
}

void GeneralPage::onServiceUnregistered(const QString& service)
{
    Q_UNUSED(service);

    if (m_errorOverlay) {
        m_errorOverlay->deleteLater();
    }

    m_errorOverlay = new ErrorOverlay(this, i18n("The Power Management Service appears not to be running."),
                                      this);
}

void GeneralPage::onChargeStopThresholdChanged(int threshold)
{
    if (threshold > m_chargeStopThreshold) {
        // Only show message if there is actually a charging or fully charged battery
        const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString());
        for (const Solid::Device &device : devices) {
            const Solid::Battery *b = qobject_cast<const Solid::Battery*>(device.asDeviceInterface(Solid::DeviceInterface::Battery));
            if (b->chargeState() == Solid::Battery::Charging || b->chargeState() == Solid::Battery::FullyCharged) {
                chargeStopThresholdMessage->animatedShow();
                break;
            }
        }
    } else if (chargeStopThresholdMessage->isVisible()) {
        chargeStopThresholdMessage->animatedHide();
    }

    markAsChanged();
}

#include "GeneralPage.moc"
