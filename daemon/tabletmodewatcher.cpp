/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#include "tabletmodewatcher.h"

#include "powerdevil_debug.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto s_portalService = "org.freedesktop.portal.Desktop"_L1;
constexpr auto s_portalPath = "/org/freedesktop/portal/desktop"_L1;
constexpr auto s_portalSettingsInterface = "org.freedesktop.portal.Settings"_L1;
constexpr auto s_tabletModeGroup = "org.kde.TabletMode"_L1;
constexpr auto s_enabledKey = "enabled"_L1;

// A session can say what form factor to take instead of leaving it to the hardware, which is what
// a phone or tablet image does and what makes the mobile behaviour reachable on a desktop. The
// names are the ones the whole session goes by, so that every part of it agrees.
bool sessionSaysTabletMode(bool *saidSo)
{
    const auto saysYes = [](const char *name) {
        const QString value = QString::fromLatin1(qgetenv(name));
        return value == "1"_L1 || value == "true"_L1;
    };

    *saidSo = qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MOBILE") || qEnvironmentVariableIsSet("KDE_KIRIGAMI_TABLET_MODE");
    return saysYes("QT_QUICK_CONTROLS_MOBILE") || saysYes("KDE_KIRIGAMI_TABLET_MODE");
}
}

namespace PowerDevil
{

TabletModeWatcher::TabletModeWatcher(QObject *parent)
    : QObject(parent)
{
    bool sessionSaidSo = false;
    const bool tabletMode = sessionSaysTabletMode(&sessionSaidSo);
    if (sessionSaidSo) {
        m_tabletMode = tabletMode;
        return;
    }
    if (qEnvironmentVariableIsSet("QT_NO_XDG_DESKTOP_PORTAL")) {
        return;
    }

    QDBusConnection::sessionBus().connect(s_portalService,
                                          s_portalPath,
                                          s_portalSettingsInterface,
                                          u"SettingChanged"_s,
                                          this,
                                          SLOT(onPortalSettingChanged(QString, QString, QDBusVariant)));

    QDBusMessage message = QDBusMessage::createMethodCall(s_portalService, s_portalPath, s_portalSettingsInterface, u"Read"_s);
    message << s_tabletModeGroup << s_enabledKey;

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        watcher->deleteLater();
        QDBusPendingReply<QDBusVariant> reply = *watcher;
        if (reply.isError()) {
            // A session without a portal, or one whose portal knows nothing of tablets, is a
            // machine that stays a laptop.
            qCDebug(POWERDEVIL) << "Could not read the tablet mode from the desktop portal:" << reply.error().message();
            return;
        }
        // Read() wraps the value in a variant of its own, which ReadOne() of newer portals does not.
        QVariant value = reply.value().variant();
        if (value.canConvert<QDBusVariant>()) {
            value = value.value<QDBusVariant>().variant();
        }
        setTabletMode(value.toBool());
    });
}

TabletModeWatcher::~TabletModeWatcher() = default;

bool TabletModeWatcher::isTabletMode() const
{
    return m_tabletMode;
}

void TabletModeWatcher::onPortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
{
    if (group != s_tabletModeGroup || key != s_enabledKey) {
        return;
    }
    setTabletMode(value.variant().toBool());
}

void TabletModeWatcher::setTabletMode(bool tabletMode)
{
    if (m_tabletMode == tabletMode) {
        return;
    }
    m_tabletMode = tabletMode;
    Q_EMIT tabletModeChanged(tabletMode);
}

}

#include "moc_tabletmodewatcher.cpp"
