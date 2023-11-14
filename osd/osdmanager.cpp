/*
    SPDX-FileCopyrightText: 2016 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redondo.de>
    SPDX-FileCopyrightText: 2023 Natalie Clarius <natalie.clarius@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "osdmanager.h"
#include "osd.h"
#include "osdaction.h"
#include "powerprofileosdserviceadaptor.h"

#include <QDBusConnection>
#include <QDBusMessage>

#include <QQmlEngine>

namespace PowerDevil
{
OsdManager::OsdManager(QObject *parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
{
    qmlRegisterUncreatableMetaObject(PowerDevil::OsdAction::staticMetaObject,
                                     "org.kde.powerdevil",
                                     1,
                                     0,
                                     "OsdAction",
                                     QStringLiteral("Can't create OsdAction"));
    new PowerProfileOsdServiceAdaptor(this);

    // free up memory when the osd hasn't been used for more than 1 minute
    m_cleanupTimer->setInterval(60000);
    m_cleanupTimer->setSingleShot(true);
    connect(m_cleanupTimer, &QTimer::timeout, this, [this]() {
        quit();
    });
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/powerdevil/powerProfileOsdService"), this, QDBusConnection::ExportAdaptors);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.powerdevil.powerProfileOsdService"));
}

void OsdManager::hideOsd()
{
    // Let QML engine finish execution of signal handlers, if any.
    QTimer::singleShot(0, this, &OsdManager::quit);
}

void OsdManager::quit()
{
    qApp->quit();
}

OsdManager::~OsdManager()
{
}

void OsdManager::showOsd()
{
    QDBusMessage message1 = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                           QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                           QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                           QStringLiteral("currentProfile"));
    auto reply1 = QDBusConnection::sessionBus().call(message1);
    if (reply1.type() == QDBusMessage::ErrorMessage) {
        return;
    }
    QString currentProfile = reply1.arguments().first().toString();

    QDBusMessage message2 = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                           QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                           QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                           QStringLiteral("profileChoices"));
    auto reply2 = QDBusConnection::sessionBus().call(message2);
    if (reply2.type() == QDBusMessage::ErrorMessage) {
        return;
    }
    QStringList availableProfiles = reply2.arguments().first().toStringList();

    PowerDevil::Osd * osd = new PowerDevil::Osd(this);
    osd->showActionSelector(availableProfiles, currentProfile);

    connect(osd, &Osd::osdActionSelected, this, [this](QString profile) {
        applyProfile(profile);
        hideOsd();
    });
}

void OsdManager::applyProfile(QString profile) {
    if (profile.isEmpty()) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                          QStringLiteral("/org/kde/Solid/PowerManagement/Actions/PowerProfile"),
                                                          QStringLiteral("org.kde.Solid.PowerManagement.Actions.PowerProfile"),
                                                          QStringLiteral("setProfile"));
    message.setArguments({profile});
    QDBusConnection::sessionBus().call(message);
}

}

#include "moc_osdmanager.cpp"
