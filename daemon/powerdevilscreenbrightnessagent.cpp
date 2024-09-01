/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2024 Jakob Petsovits <jpetso@petsovits.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "powerdevilscreenbrightnessagent.h"

#include <powerdevil_debug.h>
#include <screenbrightnessadaptor.h>
#include <screenbrightnessdisplayadaptor.h>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDebug>

#include <KScreen/Output>

using namespace Qt::Literals::StringLiterals;

static const QString DBUS_PROPERTIES_IFACE = u"org.freedesktop.DBus.Properties"_s;
static const QString SCREENBRIGHTNESS_PATH = u"/org/kde/ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_IFACE = u"org.kde.ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_DISPLAY_PATH_TEMPLATE = u"/org/kde/ScreenBrightness/%1"_s;
static const QString SCREENBRIGHTNESS_DISPLAY_DBUS_NAME_TEMPLATE = u"display%1"_s; // same as above, last path element only

namespace PowerDevil
{
ScreenBrightnessDisplay::ScreenBrightnessDisplay(QObject *parent, const QString &dbusName, ScreenBrightnessController *controller, const QString &displayId)
    : QObject(parent)
    , m_dbusName(dbusName)
    , m_displayId(displayId)
    , m_controller(controller)
{
    new ScreenBrightnessDisplayAdaptor(this);
    QDBusConnection::sessionBus().registerObject(dbusPath(), this);
}

ScreenBrightnessDisplay::~ScreenBrightnessDisplay()
{
    QDBusConnection::sessionBus().unregisterObject(dbusPath());
}

QString ScreenBrightnessDisplay::displayId() const
{
    return m_displayId;
}

QString ScreenBrightnessDisplay::DBusName() const
{
    return m_dbusName;
}

QString ScreenBrightnessDisplay::dbusPath() const
{
    return SCREENBRIGHTNESS_DISPLAY_PATH_TEMPLATE.arg(m_dbusName);
}

QString ScreenBrightnessDisplay::Label() const
{
    return m_controller->label(m_displayId);
}

bool ScreenBrightnessDisplay::IsInternal() const
{
    return m_controller->isInternal(m_displayId);
}

int ScreenBrightnessDisplay::Brightness() const
{
    return m_controller->brightness(m_displayId) - m_controller->minBrightness(m_displayId);
}

int ScreenBrightnessDisplay::MaxBrightness() const
{
    return m_controller->maxBrightness(m_displayId) - m_controller->minBrightness(m_displayId);
}

void ScreenBrightnessDisplay::SetBrightness(int value, uint flags)
{
    SetBrightnessWithContext(value, flags, QString());
}

void ScreenBrightnessDisplay::SetBrightnessWithContext(int value, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(SetBrightnessFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    m_controller->setBrightness(m_displayId, value + m_controller->minBrightness(m_displayId), sourceClientName, sourceClientContext, hint);
}

ScreenBrightnessAgent::ScreenBrightnessAgent(QObject *parent, ScreenBrightnessController *controller)
    : QObject(parent)
    , m_controller(controller)
{
    new ScreenBrightnessAdaptor(this);
    QDBusConnection::sessionBus().registerObject(u"/org/kde/ScreenBrightness"_s, this);

    connect(m_controller, &ScreenBrightnessController::brightnessChanged, this, &ScreenBrightnessAgent::onBrightnessChanged);
    // TODO: handle max brightness change via new ScreenBrightnessController::brightnessRangeChanged signal?

    connect(m_controller, &ScreenBrightnessController::displayAdded, this, [this](const QString &displayId) {
        const QString dbusName = insertDisplayChild(displayId);
        Q_EMIT DisplayAdded(dbusName);
    });
    connect(m_controller, &ScreenBrightnessController::displayRemoved, this, [this](const QString &displayId) {
        auto displayIt = m_displayChildren.find(displayId);
        if (displayIt == m_displayChildren.end()) {
            qCWarning(POWERDEVIL) << "onDisplayRemoved: Agent could not find display object for ID" << displayId;
            return;
        }
        const QString dbusName = displayIt->second->DBusName();

        m_displayChildren.erase(displayIt);
        Q_EMIT DisplayRemoved(dbusName);
    });

    const QStringList initialDisplayIds = m_controller->displayIds();
    std::ranges::for_each(initialDisplayIds, std::bind(&ScreenBrightnessAgent::insertDisplayChild, this, std::placeholders::_1));

    connect(m_controller, &ScreenBrightnessController::displayIdsChanged, this, [this](const QStringList &) {
        QDBusMessage signal = QDBusMessage::createSignal(SCREENBRIGHTNESS_PATH, DBUS_PROPERTIES_IFACE, u"PropertiesChanged"_s);
        signal << SCREENBRIGHTNESS_IFACE << QVariantMap() << QStringList({u"DisplaysDBusNames"_s});
        QDBusConnection::sessionBus().send(signal);
    });
}

QString ScreenBrightnessAgent::insertDisplayChild(const QString &displayId)
{
    const size_t dbusDisplayIndex = m_nextDbusDisplayIndex;
    ++m_nextDbusDisplayIndex;

    const auto &[it, wasInserted] = m_displayChildren.insert_or_assign(
        displayId,
        std::make_unique<ScreenBrightnessDisplay>(nullptr, SCREENBRIGHTNESS_DISPLAY_DBUS_NAME_TEMPLATE.arg(dbusDisplayIndex), m_controller, displayId));

    return it->second->DBusName();
}

void ScreenBrightnessAgent::onBrightnessChanged(const QString &displayId,
                                                const BrightnessLogic::BrightnessInfo &info,
                                                const QString &sourceClientName,
                                                const QString &sourceClientContext,
                                                ScreenBrightnessController::IndicatorHint hint)
{
    auto displayIt = m_displayChildren.find(displayId);
    if (displayIt == m_displayChildren.end()) {
        qCWarning(POWERDEVIL) << "onBrightnessChanged: Agent could not find display object for ID" << displayId;
        return;
    }
    int newBrightness = info.value - info.valueMin;

    Q_EMIT BrightnessChanged(displayIt->second->DBusName(), newBrightness, sourceClientName, sourceClientContext);

    QDBusMessage signal = QDBusMessage::createSignal(displayIt->second->dbusPath(), DBUS_PROPERTIES_IFACE, u"PropertiesChanged"_s);
    QVariantMap changedProps = {{u"Brightness"_s, newBrightness}};
    signal << u"org.kde.ScreenBrightness.Display"_s << changedProps << QStringList();
    QDBusConnection::sessionBus().send(signal);

    if (hint == ScreenBrightnessController::ShowIndicator) {
        // Try to match the controller's display to a KScreen display for optimized OSD presentation.
        const KScreen::OutputPtr matchedOutput = m_controller->tryMatchKScreenOutput(displayId);

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QLatin1String("screenBrightnessChanged"));
        msg << brightnessPercent(newBrightness, info.valueMax - info.valueMin) << (matchedOutput ? matchedOutput->name() : displayId)
            << m_controller->label(displayId) << static_cast<int>(m_controller->displayIds().indexOf(displayId))
            << (matchedOutput && matchedOutput->isPositionable() ? matchedOutput->geometry() : QRect());
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

void ScreenBrightnessAgent::onBrightnessRangeChanged(const QString &displayId, const BrightnessLogic::BrightnessInfo &info)
{
    auto displayIt = m_displayChildren.find(displayId);
    if (displayIt == m_displayChildren.end()) {
        qCWarning(POWERDEVIL) << "onBrightnessRangeChanged: Agent could not find display object for ID" << displayId;
        return;
    }

    int newBrightness = info.value - info.valueMin;
    int newMaxBrightness = info.valueMax - info.valueMin;

    Q_EMIT BrightnessRangeChanged(displayIt->second->DBusName(), newMaxBrightness, newBrightness);

    QDBusMessage signal = QDBusMessage::createSignal(displayIt->second->dbusPath(), DBUS_PROPERTIES_IFACE, u"PropertiesChanged"_s);
    QVariantMap changedProps = {{u"Brightness"_s, newBrightness}, {u"MaxBrightness"_s, newMaxBrightness}};
    signal << u"org.kde.ScreenBrightness.Display"_s << changedProps << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

QStringList ScreenBrightnessAgent::DisplaysDBusNames() const
{
    QStringList names;
    std::ranges::transform(m_controller->displayIds(), std::back_inserter(names), [this](const QString &displayId) {
        if (auto it = m_displayChildren.find(displayId); it != m_displayChildren.end()) {
            return it->second->DBusName();
        } else {
            qCWarning(POWERDEVIL) << "DisplaysDBusNames: Agent could not find display object for ID" << displayId;
            return QString();
        }
    });
    return names;
}

void ScreenBrightnessAgent::AdjustBrightnessRatio(double delta, uint flags)
{
    AdjustBrightnessRatioWithContext(delta, flags, QString());
}

void ScreenBrightnessAgent::AdjustBrightnessRatioWithContext(double delta, uint flags, const QString &sourceClientContext)
{
    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(AdjustBrightnessRatioFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    m_controller->adjustBrightnessRatio(delta, sourceClientName, sourceClientContext, hint);
}

void ScreenBrightnessAgent::AdjustBrightnessStep(uint stepAction, uint flags)
{
    AdjustBrightnessStepWithContext(stepAction, flags, QString());
}

void ScreenBrightnessAgent::AdjustBrightnessStepWithContext(uint stepAction, uint flags, const QString &sourceClientContext)
{
    BrightnessLogic::StepAdjustmentAction adjustment;
    switch (static_cast<AdjustBrightnessStepAction>(stepAction)) {
    case AdjustBrightnessStepAction::Increase:
        adjustment = BrightnessLogic::Increase;
        break;
    case AdjustBrightnessStepAction::Decrease:
        adjustment = BrightnessLogic::Decrease;
        break;
    case AdjustBrightnessStepAction::IncreaseSmall:
        adjustment = BrightnessLogic::IncreaseSmall;
        break;
    case AdjustBrightnessStepAction::DecreaseSmall:
        adjustment = BrightnessLogic::DecreaseSmall;
        break;
    default:
        qCDebug(POWERDEVIL) << "Adjust brightness step: Unknown step action:" << stepAction;
        return;
    }

    ScreenBrightnessController::IndicatorHint hint = flags & static_cast<uint>(AdjustBrightnessStepFlags::SuppressIndicator)
        ? ScreenBrightnessController::SuppressIndicator
        : ScreenBrightnessController::ShowIndicator;

    QString sourceClientName = message().service();

    m_controller->adjustBrightnessStep(adjustment, sourceClientName, sourceClientContext, hint);
}

int ScreenBrightnessAgent::brightnessPercent(double value, double max) const
{
    return max > 0 ? std::round(value / max * 100) : 0;
}
}

#include "moc_powerdevilscreenbrightnessagent.cpp"
