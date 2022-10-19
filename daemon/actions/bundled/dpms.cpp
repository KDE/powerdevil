/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
 *   Copyright (C) 2015 by Kai Uwe Broulik <kde@privat.broulik.de>         *
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


#include "dpms.h"
#include <KScreenDpms/Dpms>

#include <powerdevilbackendinterface.h>
#include <powerdevilcore.h>
#include <powerdevil_debug.h>
#include <kwinkscreenhelpereffect.h>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QGuiApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <private/qtx11extras_p.h>
#endif
#include <QTimer>
#include <QDebug>

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <Kirigami/TabletModeWatcher>

K_PLUGIN_CLASS_WITH_JSON(PowerDevil::BundledActions::DPMS, "powerdevildpmsaction.json")

namespace PowerDevil {
namespace BundledActions {
DPMS::DPMS(QObject *parent, const QVariantList &)
    : Action(parent)
    , m_dpms(new KScreen::Dpms)
{
    setRequiredPolicies(PowerDevil::PolicyAgent::ChangeScreenSettings);

    // On Wayland, KWin takes care of performing the effect properly
    if (QX11Info::isPlatformX11()) {
        auto fadeEffect = new PowerDevil::KWinKScreenHelperEffect(this);
        connect(this, &DPMS::startFade, fadeEffect, &PowerDevil::KWinKScreenHelperEffect::start);
        connect(this, &DPMS::stopFade, fadeEffect, &PowerDevil::KWinKScreenHelperEffect::stop);
    }

    // Listen to the policy agent
    connect(PowerDevil::PolicyAgent::instance(), &PowerDevil::PolicyAgent::unavailablePoliciesChanged,
            this, &DPMS::onUnavailablePoliciesChanged);

    // inhibitions persist over kded module unload/load
    m_inhibitScreen = PowerDevil::PolicyAgent::instance()->unavailablePolicies() & PowerDevil::PolicyAgent::ChangeScreenSettings;

    KActionCollection *actionCollection = new KActionCollection( this );
    actionCollection->setComponentDisplayName(i18nc("Name for powerdevil shortcuts category", "Power Management"));

    QAction *globalAction = actionCollection->addAction(QLatin1String("Turn Off Screen"));
    globalAction->setText(i18nc("@action:inmenu Global shortcut", "Turn Off Screen"));
    connect(globalAction, &QAction::triggered, this, [this] {
        if (m_lockBeforeTurnOff) {
            lockScreen();
        }
        m_dpms->switchMode(KScreen::Dpms::Off);
    });

    auto powerButtonMode = [globalAction] (bool isTablet) {
        if (!isTablet) {
            KGlobalAccel::self()->setGlobalShortcut(globalAction, QList<QKeySequence>());
        } else {
            KGlobalAccel::self()->setGlobalShortcut(globalAction, Qt::Key_PowerOff);
        }
    };
    auto interface = Kirigami::TabletModeWatcher::self();
    connect(interface, &Kirigami::TabletModeWatcher::tabletModeChanged, globalAction, powerButtonMode);
    powerButtonMode(interface->isTabletMode());
}

DPMS::~DPMS() = default;

bool DPMS::isSupported()
{
    return m_dpms->isSupported();
}

void DPMS::onWakeupFromIdle()
{
    if (isSupported()) {
        Q_EMIT stopFade();
    }
    if (m_oldKeyboardBrightness > 0) {
        setKeyboardBrightnessHelper(m_oldKeyboardBrightness);
        m_oldKeyboardBrightness = 0;
    }
}

void DPMS::onIdleTimeout(int msec)
{
    // Do not inhibit anything even if idleTimeout reaches because we are inhibit
    if (m_inhibitScreen) {
        return;
    }

    if (msec == m_idleTime * 1000 - 5000) { // fade out screen
        if (isSupported()) {
            Q_EMIT startFade();
        }
    } else if (msec == m_idleTime * 1000) {
        const int brightness = backend()->brightness(PowerDevil::BackendInterface::Keyboard);
        if (brightness > 0) {
            m_oldKeyboardBrightness = brightness;
            setKeyboardBrightnessHelper(0);
        }
        if (isSupported()) {
            m_dpms->switchMode(KScreen::Dpms::Off);
        }
    }
}

void DPMS::setKeyboardBrightnessHelper(int brightness)
{
    trigger({
        {"KeyboardBrightness", QVariant::fromValue(brightness)}
    });
}

void DPMS::triggerImpl(const QVariantMap& args)
{
    QString KEYBOARD_BRIGHTNESS = QStringLiteral("KeyboardBrightness");
    if (args.contains(KEYBOARD_BRIGHTNESS)) {
        backend()->setBrightness(args.value(KEYBOARD_BRIGHTNESS).toInt(), PowerDevil::BackendInterface::Keyboard);
        return;
    }

    if (!isSupported()) {
        return;
    }
    QString type = args.value(QStringLiteral("Type")).toString();
    if (m_lockBeforeTurnOff && (type == "TurnOff" || type == "ToggleOnOff")) {
        lockScreen();
    }
    KScreen::Dpms::Mode level = KScreen::Dpms::Mode::On;
    if (type == QLatin1String("ToggleOnOff")) {
        level = KScreen::Dpms::Toggle;
    } else if (type == QLatin1String("TurnOff")) {
        level = KScreen::Dpms::Off;
    } else if (type == QLatin1String("Standby")) {
        level = KScreen::Dpms::Standby;
    } else if (type == QLatin1String("Suspend")) {
        level = KScreen::Dpms::Suspend;
    } else {
        level = KScreen::Dpms::On;
    }
    m_dpms->switchMode(level);
}

bool DPMS::loadAction(const KConfigGroup& config)
{
    m_idleTime = config.readEntry<int>("idleTime", -1);
    if (m_idleTime > 0) {
        registerIdleTimeout(m_idleTime * 1000);
        registerIdleTimeout(m_idleTime * 1000 - 5000); // start screen fade a bit earlier to alert user
    }
    m_lockBeforeTurnOff = config.readEntry<bool>("lockBeforeTurnOff", false);

    return true;
}

bool DPMS::onUnloadAction()
{
    m_idleTime = 0;
    return Action::onUnloadAction();
}

void DPMS::onUnavailablePoliciesChanged(PowerDevil::PolicyAgent::RequiredPolicies policies)
{
    m_inhibitScreen = policies & PowerDevil::PolicyAgent::ChangeScreenSettings;
}

static std::chrono::milliseconds dimAnimationTime()
{
    // See kscreen.kcfg from kwin
    return std::chrono::milliseconds (KSharedConfig::openConfig("kwinrc")->group("Effect-Kscreen").readEntry("Duration", 250));
}

void DPMS::lockScreen()
{
    // We need to delay locking until the screen has dimmed, otherwise it looks all clunky
    QTimer::singleShot(dimAnimationTime(), [] {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall("org.freedesktop.ScreenSaver",
                                                                            "/ScreenSaver",
                                                                            "org.freedesktop.ScreenSaver",
                                                                            "Lock"));
    });
}

}
}
#include "dpms.moc"
