/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenbrightnesscontrol.h"

#include <QCoroDBusPendingCall>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace
{
static const QString SCREENBRIGHTNESS_SERVICE = u"org.kde.ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_PATH = u"/org/kde/ScreenBrightness"_s;
static const QString SCREENBRIGHTNESS_IFACE = u"org.kde.ScreenBrightness"_s;
static const QString ALREADY_CHANGED_CONTEXT = u"AlreadyChanged"_s;
}

ScreenBrightnessControl::ScreenBrightnessControl(QObject *parent)
    : QObject(parent)
{
    init();
}

ScreenBrightnessControl::~ScreenBrightnessControl()
{
}

ScreenBrightnessDisplayModel *ScreenBrightnessControl::displays()
{
    return &m_displays;
}

const ScreenBrightnessDisplayModel *ScreenBrightnessControl::displays() const
{
    return &m_displays;
}

void ScreenBrightnessControl::adjustBrightnessRatio(double delta)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"AdjustBrightnessRatio"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << delta << flags;
    QDBusConnection::sessionBus().asyncCall(msg);
}

void ScreenBrightnessControl::adjustBrightnessStep(ScreenBrightnessControl::StepAction stepAction)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"AdjustBrightnessStep"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << qToUnderlying(stepAction) << flags;
    QDBusConnection::sessionBus().asyncCall(msg);
}

void ScreenBrightnessControl::setBrightness(const QString &displayId, int value)
{
    QVariant oldBrightness = m_displays.data(m_displays.displayIndex(displayId), ScreenBrightnessDisplayModel::BrightnessRole);

    if (oldBrightness == value) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"SetBrightnessWithContext"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << displayId << value << flags << ALREADY_CHANGED_CONTEXT;
    QDBusPendingCall async = QDBusConnection::sessionBus().asyncCall(msg);
    m_brightnessChangeWatcher.reset(new QDBusPendingCallWatcher(async));

    connect(m_brightnessChangeWatcher.get(),
            &QDBusPendingCallWatcher::finished,
            this,
            [this, displayId, oldValue = oldBrightness.toInt()](QDBusPendingCallWatcher *watcher) {
                const QDBusPendingReply<void> reply = *watcher;
                if (reply.isError()) {
                    qDebug() << "error setting brightness via dbus" << reply.error();
                    m_displays.onBrightnessChanged(displayId, oldValue);
                }
                m_brightnessChangeWatcher.reset();
            });

    m_displays.onBrightnessChanged(displayId, value);
}

void ScreenBrightnessControl::onBrightnessChanged(const QString &displayId, int value, const QString &sourceClientName, const QString &sourceClientContext)
{
    if (sourceClientName == QDBusConnection::sessionBus().baseService() && sourceClientContext == ALREADY_CHANGED_CONTEXT) {
        qDebug() << "ignoring brightness change, it's coming from the applet itself";
        return;
    }
    m_displays.onBrightnessChanged(displayId, value);
}

void ScreenBrightnessControl::onBrightnessRangeChanged(const QString &displayId, int max, int value)
{
    m_displays.onBrightnessRangeChanged(displayId, max, value);

    QVariant firstDisplayMax = m_displays.data(m_displays.index(0, 0), ScreenBrightnessDisplayModel::MaxBrightnessRole);

    m_isBrightnessAvailable = firstDisplayMax.isValid() && firstDisplayMax.toInt() > 0;
}

void ScreenBrightnessControl::onDisplayAdded(const QString &displayId)
{
    queryAndAppendDisplay(displayId);
}

void ScreenBrightnessControl::onDisplayRemoved(const QString &displayId)
{
    m_displays.removeDisplay(displayId);
}

QCoro::Task<void> ScreenBrightnessControl::init()
{
    QDBusMessage displayIdsMsg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"GetDisplayIds"_s);
    QPointer<ScreenBrightnessControl> alive{this};
    const QDBusReply<QList<QString>> displayIdsReply = co_await QDBusConnection::sessionBus().asyncCall(displayIdsMsg);
    if (!alive || !displayIdsReply.isValid()) {
        qDebug() << "error getting display ids via dbus:" << displayIdsReply.error();
        co_return;
    }
    QStringList displayIds = displayIdsReply.value();

    for (const QString &displayId : std::as_const(displayIds)) {
        queryAndAppendDisplay(displayId);
    }

    if (!QDBusConnection::sessionBus().connect(SCREENBRIGHTNESS_SERVICE,
                                               SCREENBRIGHTNESS_PATH,
                                               SCREENBRIGHTNESS_IFACE,
                                               u"BrightnessChanged"_s,
                                               this,
                                               SLOT(onBrightnessChanged(QString, int, QString, QString)))) {
        qDebug() << "error connecting to Brightness changes via dbus";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SCREENBRIGHTNESS_SERVICE,
                                               SCREENBRIGHTNESS_PATH,
                                               SCREENBRIGHTNESS_IFACE,
                                               u"BrightnessRangeChanged"_s,
                                               this,
                                               SLOT(onBrightnessRangeChanged(QString, int, int)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    if (!QDBusConnection::sessionBus()
             .connect(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"DisplayAdded"_s, this, SLOT(onDisplayAdded(QString)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    if (!QDBusConnection::sessionBus()
             .connect(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"DisplayRemoved"_s, this, SLOT(onDisplayRemoved(QString)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    m_isBrightnessAvailable = true;
}

QCoro::Task<void> ScreenBrightnessControl::queryAndAppendDisplay(const QString &displayId)
{
    QDBusMessage labelMsg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"GetLabel"_s);
    labelMsg << displayId;
    QPointer<ScreenBrightnessControl> alive{this};
    const QDBusReply<QString> labelReply = QDBusConnection::sessionBus().call(labelMsg);
    if (!alive || !labelReply.isValid()) {
        qDebug() << "error getting display label via dbus:" << labelReply.error();
        co_return;
    }
    QString label = labelReply.value();

    QDBusMessage isInternalMsg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"GetIsInternal"_s);
    isInternalMsg << displayId;
    const QDBusReply<bool> isInternalReply = QDBusConnection::sessionBus().call(isInternalMsg);
    if (!alive || !isInternalReply.isValid()) {
        qDebug() << "error getting display is-internal via dbus:" << isInternalReply.error();
        co_return;
    }
    bool isInternal = isInternalReply.value();

    QDBusMessage maxBrightnessMsg =
        QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"GetMaxBrightness"_s);
    maxBrightnessMsg << displayId;
    const QDBusReply<int> maxBrightnessReply = QDBusConnection::sessionBus().call(maxBrightnessMsg);
    if (!alive || !maxBrightnessReply.isValid()) {
        qDebug() << "error getting max screen brightness via dbus:" << maxBrightnessReply.error();
        co_return;
    }
    int maxBrightness = maxBrightnessReply.value();

    QDBusMessage brightnessMsg = QDBusMessage::createMethodCall(SCREENBRIGHTNESS_SERVICE, SCREENBRIGHTNESS_PATH, SCREENBRIGHTNESS_IFACE, u"GetBrightness"_s);
    brightnessMsg << displayId;
    const QDBusReply<int> brightnessReply = QDBusConnection::sessionBus().call(brightnessMsg);
    if (!alive || !brightnessReply.isValid()) {
        qDebug() << "error getting screen brightness via dbus:" << brightnessReply.error();
        co_return;
    }
    int brightness = brightnessReply.value();

    m_displays.appendDisplay(displayId, label, isInternal, brightness, maxBrightness);
}

QBindable<bool> ScreenBrightnessControl::bindableIsBrightnessAvailable()
{
    return &m_isBrightnessAvailable;
}

#include "moc_screenbrightnesscontrol.cpp"
