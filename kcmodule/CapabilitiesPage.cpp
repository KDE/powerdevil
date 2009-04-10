/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#include "CapabilitiesPage.h"

#include "PowerDevilSettings.h"

#include <config-X11.h>
#include <config-workspace.h>
#include <config-powerdevil.h>

#include <solid/control/powermanager.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/processor.h>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>

#include <QX11Info>

#include <QFormLayout>
#include <QProcess>
#include <KPushButton>
#include <KMessageBox>

#ifdef HAVE_DPMS
#include <X11/Xmd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
extern "C" {
#include <X11/extensions/dpms.h>
    int __kde_do_not_unload = 1;

#ifndef HAVE_DPMSCAPABLE_PROTO
    Bool DPMSCapable(Display *);
#endif

#ifndef HAVE_DPMSINFO_PROTO
    Status DPMSInfo(Display *, CARD16 *, BOOL *);
#endif
}
#endif

CapabilitiesPage::CapabilitiesPage(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    fillUi();
}

CapabilitiesPage::~CapabilitiesPage()
{
}

void CapabilitiesPage::fillUi()
{
}

void CapabilitiesPage::load()
{
    fillCapabilities();
}

void CapabilitiesPage::fillCapabilities()
{
    int batteryCount = 0;
    int cpuCount = 0;

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString())) {
        Q_UNUSED(device)
        batteryCount++;
    }

    foreach(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString())) {
        /*Solid::Device d = device;
        Solid::Processor *processor = qobject_cast<Solid::Processor*>(d.asDeviceInterface(Solid::DeviceInterface::Processor));

        if (processor->canChangeFrequency()) {
            freqchange = true;
        }*/
        Q_UNUSED(device)
        ++cpuCount;
    }

    cpuNumber->setText(QString::number(cpuCount));
    batteriesNumber->setText(QString::number(batteryCount));

    bool turnOff = false;

    for (int i = 0; i < cpuCount; ++i) {
        if (Solid::Control::PowerManager::canDisableCpu(i))
            turnOff = true;
    }

    if (turnOff)
        isCPUOffSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    else
        isCPUOffSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));

    QString sMethods;

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if (methods & Solid::Control::PowerManager::ToDisk) {
        sMethods.append(QString(i18n("Suspend to Disk") + QString(", ")));
    }

    if (methods & Solid::Control::PowerManager::ToRam) {
        sMethods.append(QString(i18n("Suspend to RAM") + QString(", ")));
    }

    if (methods & Solid::Control::PowerManager::Standby) {
        sMethods.append(QString(i18n("Standby") + QString(", ")));
    }

    if (!sMethods.isEmpty()) {
        sMethods.remove(sMethods.length() - 2, 2);
    } else {
        sMethods = i18nc("None", "No methods found");
    }

    supportedMethods->setText(sMethods);

    QString scMethods;

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if (policies & Solid::Control::PowerManager::Performance) {
        scMethods.append(QString(i18n("Performance") + QString(", ")));
    }

    if (policies & Solid::Control::PowerManager::OnDemand) {
        scMethods.append(QString(i18n("Dynamic (ondemand)") + QString(", ")));
    }

    if (policies & Solid::Control::PowerManager::Conservative) {
        scMethods.append(QString(i18n("Dynamic (conservative)") + QString(", ")));
    }

    if (policies & Solid::Control::PowerManager::Powersave) {
        scMethods.append(QString(i18n("Powersave") + QString(", ")));
    }

    if (policies & Solid::Control::PowerManager::Userspace) {
        scMethods.append(QString(i18n("Userspace") + QString(", ")));
    }

    if (!scMethods.isEmpty()) {
        scMethods.remove(scMethods.length() - 2, 2);
        isScalingSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        scMethods = i18nc("None", "No methods found");
        isScalingSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    supportedPolicies->setText(scMethods);

    if (!Solid::Control::PowerManager::supportedSchemes().isEmpty()) {
        isSchemeSupported->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        isSchemeSupported->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    QString schemes;

    foreach(const QString &scheme, Solid::Control::PowerManager::supportedSchemes()) {
        schemes.append(scheme + QString(", "));
    }

    if (!schemes.isEmpty()) {
        schemes.remove(schemes.length() - 2, 2);
    } else {
        schemes = i18nc("None", "No methods found");
    }

    supportedSchemes->setText(schemes);

    bool xss = false;
    bool xsync = false;
    bool xtest = false;
    bool dpms = false;

#ifdef HAVE_DPMS
    Display *dpy = QX11Info::display();

    int dummy;

    if (DPMSQueryExtension(dpy, &dummy, &dummy) && DPMSCapable(dpy)) {
        dpms = true;
    }
#endif

    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded",
                       "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedPollingSystems");
    QDBusReply<QVariantMap> systems = QDBusConnection::sessionBus().call(msg);

    foreach(const QVariant &ent, systems.value()) {
        if (ent.toInt() == XSyncBased) {
            xsync = true;
        } else if (ent.toInt() == WidgetBased) {
            xss = true;
        }
    }

#ifdef HAVE_XTEST
    xtest = true;
#endif

    bool ck;

    if (!QDBusConnection::systemBus().interface()->isServiceRegistered("org.freedesktop.ConsoleKit")) {
        // No way to determine if we are on the current session, simply suppose we are
        ck = false;
    } else {
        QDBusInterface ckiface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager",
                               "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus());

        QDBusReply<QDBusObjectPath> sessionPath = ckiface.call("GetCurrentSession");

        if (!sessionPath.isValid() || sessionPath.value().path().isEmpty()) {
            kDebug() << "The session is not registered with ck";
            ck = false;
        } else {
            QDBusInterface ckSessionInterface("org.freedesktop.ConsoleKit", sessionPath.value().path(),
                                              "org.freedesktop.ConsoleKit.Session", QDBusConnection::systemBus());

            if (!ckSessionInterface.isValid()) {
                ck = false;
            } else {
                ck = true;
            }
        }
    }

    if (xss) {
        xssSupport->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        xssSupport->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    if (xsync) {
        xsyncSupport->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        xsyncSupport->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    if (xtest) {
        xtestSupport->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        xtestSupport->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    if (dpms) {
        dpmsSupport->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        dpmsSupport->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    if (ck) {
        ckSupport->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
    } else {
        ckSupport->setPixmap(KIcon("dialog-cancel").pixmap(16, 16));
    }

    // Determine status!

    foreach(QObject *ent, statusLayout->children()) {
        ent->deleteLater();
    }

    if (!xss & !xsync) {
        setIssue(true, i18n("PowerDevil was compiled without Xss and Xext support, or the XSync "
                            "extension is not available. Determining idle time will not be possible. "
                            "Please consider recompiling PowerDevil with at least one of these two "
                            "libraries."));
    } else if ((scMethods.isEmpty() || scMethods == i18n("Performance")) &&
               PowerDevilSettings::scalingWarning()) {
        setIssue(true, i18n("No scaling methods were found. If your CPU is reasonably recent, this "
                            "is probably because you have not loaded some kernel modules. Usually "
                            "scaling modules have names similar to cpufreq_ondemand. Scaling is "
                            "useful and can save a lot of battery. Click on \"Attempt Loading Modules\" "
                            "to let PowerDevil try to load the required modules. If you are sure your PC "
                            "does not support scaling, you can also disable this warning by clicking "
                            "\"Do not display this warning again\"."),
                 i18n("Attempt to load modules"), "system-run", SLOT(attemptLoadingModules()),
                 i18n("Do not display this warning again"), "dialog-ok-apply", SLOT(disableScalingWarn()));

    } else if (!xsync) {
        setIssue(true, i18n("PowerDevil was compiled without Xext support, or the XSync extension is "
                            "not available. XSync grants extra efficiency and performance, saving your "
                            "battery and CPU. It is advised to use PowerDevil with XSync enabled."));

    } else if (PowerDevilSettings::pollingSystem() != 2 && xsync) {
        setIssue(true, i18n("XSync does not seem to be your preferred query backend, though it is available "
                            "on your system. Using it largely improves performance and efficiency, and "
                            "is strongly advised. Click on the button below to enable it now."),
                 i18n("Enable XSync Backend"), "dialog-ok-apply", SLOT(enableXSync()));
    } else if (!ck) {
        setIssue(true, i18n("ConsoleKit was not found active on your PC, or PowerDevil can not contact it. "
                            "ConsoleKit lets PowerDevil detect if the current session is active, hence it is "
                            "very useful if you abitually have more than an user logged into your system at "
                            "the same time."));

    } else {
        setIssue(false, i18n("No issues found with your configuration."));
    }
}

void CapabilitiesPage::setIssue(bool issue, const QString &text,
                                const QString &button, const QString &buticon, const char *slot,
                                const QString &button2, const QString &buticon2, const char *slot2)
{
    QLabel *pm = new QLabel(this);
    QLabel *tx = new QLabel(this);
    QHBoxLayout *ly = new QHBoxLayout();
    QHBoxLayout *butly = 0;

    pm->setMaximumWidth(30);
    tx->setScaledContents(true);
    tx->setWordWrap(true);

    ly->addWidget(pm);
    ly->addWidget(tx);

    if (!issue) {
        pm->setPixmap(KIcon("dialog-ok-apply").pixmap(16, 16));
        tx->setText(text);
        emit issuesFound(false);
    } else {
        pm->setPixmap(KIcon("dialog-warning").pixmap(16, 16));
        tx->setText(text);

        if (!button.isEmpty()) {
            butly = new QHBoxLayout();
            KPushButton *but = new KPushButton(this);

            but->setText(button);
            but->setIcon(KIcon(buticon));

            ly->removeWidget(tx);

            butly->addStretch();
            butly->addWidget(but);

            QVBoxLayout *vl = new QVBoxLayout();

            vl->addWidget(tx);
            vl->addLayout(butly);

            ly->addLayout(vl);

            connect(but, SIGNAL(clicked()), slot);
        }
        if (!button2.isEmpty()) {
            KPushButton *but = new KPushButton(this);

            but->setText(button2);
            but->setIcon(KIcon(buticon2));

            butly->addWidget(but);

            connect(but, SIGNAL(clicked()), slot2);
        }
        if (!button.isEmpty()) {
            butly->addStretch();
        }

        emit issuesFound(true);
    }

    statusLayout->addLayout(ly);
}

void CapabilitiesPage::disableScalingWarn()
{
    PowerDevilSettings::setScalingWarning(false);

    emit reload();
    emit reloadModule();
}

void CapabilitiesPage::attemptLoadingModules()
{
    // Let's check what we have

    QProcess process;

    process.start("modprobe -l");
    process.waitForFinished();

    QStringList modules;

    foreach(const QString &ent, process.readAll().split('\n')) {
        if (ent.contains("cpufreq_") || ent.contains("ondemand")) {
            QStringList ents = ent.split('/');
            QString module = ents.at(ents.count() - 1);
            module.remove(module.length() - 3, 3);
            modules.append(module);
        }
    }

    if (modules.isEmpty()) {
        KMessageBox::sorry(this, i18n("No kernel modules for CPU scaling were found. Either you do not "
                                      "have them installed, or PowerDevil could not detect them."),
                           i18n("Modules not found"));
        return;
    }

    QString command = "kdesu '";

    foreach(const QString &ent, modules) {
        command.append(QString("modprobe %1 | ").arg(ent));
    }

    command.remove(command.length() - 3, 3);

    command.append('\'');

    system(command.toAscii().data());

    emit reload();
    emit reloadModule();
}

void CapabilitiesPage::enableXSync()
{
    PowerDevilSettings::setPollingSystem(2);

    PowerDevilSettings::self()->writeConfig();

    emit reload();
    emit reloadModule();
}

#include "CapabilitiesPage.moc"
