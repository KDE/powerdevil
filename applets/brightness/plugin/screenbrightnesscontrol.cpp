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
constexpr QLatin1String SOLID_POWERMANAGEMENT_SERVICE("org.kde.Solid.PowerManagement");
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

QHash<QString, float> ScreenBrightnessControl::changeBrightnessByDelta(float delta, const QHash<QString, float> &trackingErrorsByDisplayId)
{
    QHash<QString, float> trackingErrors;

    for (int i = 0; i < m_displays.rowCount(); ++i) {
        QModelIndex modelIndex = m_displays.index(i, 0);
        QString displayId = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::DisplayIdRole).toString();
        int currentBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::BrightnessRole).toInt();
        int minBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::MinBrightnessRole).toInt();
        int maxBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::MaxBrightnessRole).toInt();

        double targetBrightness = currentBrightness + delta * maxBrightness;
        targetBrightness += trackingErrorsByDisplayId.value(displayId, 0.0f);

        int newBrightness = qRound(targetBrightness);
        setBrightness(displayId, qBound(minBrightness, newBrightness, maxBrightness));

        trackingErrors[displayId] = static_cast<float>(targetBrightness - newBrightness);
    }
    return trackingErrors;
}

void ScreenBrightnessControl::changeBrightnessBySteps(float stepDelta, int stepCount)
{
    for (int i = 0; i < m_displays.rowCount(); ++i) {
        QModelIndex modelIndex = m_displays.index(i, 0);
        QString displayId = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::DisplayIdRole).toString();
        int currentBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::BrightnessRole).toInt();
        int minBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::MinBrightnessRole).toInt();
        int maxBrightness = m_displays.data(modelIndex, ScreenBrightnessDisplayModel::MaxBrightnessRole).toInt();

        double stepSize = static_cast<double>(maxBrightness) / stepCount;
        double targetBrightness = (qRound(currentBrightness / stepSize) + stepDelta) * stepSize;

        int newBrightness = qRound(targetBrightness);
        setBrightness(displayId, qBound(minBrightness, newBrightness, maxBrightness));
    }
}

void ScreenBrightnessControl::setBrightness(const QString &displayId, int value)
{
    QVariant oldBrightness = m_displays.data(m_displays.displayIndex(displayId), ScreenBrightnessDisplayModel::BrightnessRole);

    if (oldBrightness == value) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(SOLID_POWERMANAGEMENT_SERVICE,
                                                      u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                      u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                      u"setBrightness"_s);
    uint flags = m_isSilent ? 0x1 : 0x0;

    msg << displayId << value << flags;
    QDBusPendingCall async = QDBusConnection::sessionBus().asyncCall(msg);
    m_brightnessChangeWatcher.reset(new QDBusPendingCallWatcher(async));

    connect(m_brightnessChangeWatcher.get(),
            &QDBusPendingCallWatcher::finished,
            this,
            [this, displayId, oldValue = oldBrightness.toInt()](QDBusPendingCallWatcher *watcher) {
                const QDBusPendingReply<void> reply = *watcher;
                if (reply.isError()) {
                    qDebug() << "error setting brightness via dbus" << reply.error();
                    onBrightnessChanged(displayId, oldValue);
                }
                m_brightnessChangeWatcher.reset();
            });

    onBrightnessChanged(displayId, value);
}

void ScreenBrightnessControl::onBrightnessChanged(const QString &displayId, int value, const QString &sourceClientName, const QString &sourceClientContext)
{
    Q_UNUSED(sourceClientContext)

    if (sourceClientName == QDBusConnection::sessionBus().baseService()) {
        qDebug() << "ignoring! it's our own source";
        return;
    }
    m_displays.onBrightnessChanged(displayId, value);
}

void ScreenBrightnessControl::onBrightnessRangeChanged(const QString &displayId, int min, int max, int value)
{
    m_displays.onBrightnessRangeChanged(displayId, min, max, value);

    QVariant firstDisplayMax = m_displays.data(m_displays.index(0, 0), ScreenBrightnessDisplayModel::MaxBrightnessRole);

    m_isBrightnessAvailable = firstDisplayMax.isValid() && firstDisplayMax.toInt() > 0;
}

void ScreenBrightnessControl::onDisplayAdded(const QString &displayId)
{
    queryAndAppendDisplay(displayId);
}

void ScreenBrightnessControl::onDisplayRemoved(const QString &displayId)
{
    qDebug() << "Removing display:" << displayId; // TODO: debugging, remove
    m_displays.removeDisplay(displayId);
}

QCoro::Task<void> ScreenBrightnessControl::init()
{
    QDBusMessage displayIdsMsg = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                                u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                                u"displayIds"_s);
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

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                               u"BrightnessChanged"_s,
                                               this,
                                               SLOT(onBrightnessChanged(QString, int, QString, QString)))) {
        qDebug() << "error connecting to Brightness changes via dbus";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                               u"BrightnessRangeChanged"_s,
                                               this,
                                               SLOT(onBrightnessRangeChanged(QString, int, int, int)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                               u"DisplayAdded"_s,
                                               this,
                                               SLOT(onDisplayAdded(QString)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    if (!QDBusConnection::sessionBus().connect(SOLID_POWERMANAGEMENT_SERVICE,
                                               u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                               u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                               u"DisplayRemoved"_s,
                                               this,
                                               SLOT(onDisplayRemoved(QString)))) {
        qDebug() << "error connecting to brightness range changes via dbus:";
        co_return;
    }

    m_isBrightnessAvailable = true;
}

QCoro::Task<void> ScreenBrightnessControl::queryAndAppendDisplay(const QString &displayId)
{
    QDBusMessage labelMsg = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                           u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                           u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                           u"label"_s);
    labelMsg << displayId;
    QPointer<ScreenBrightnessControl> alive{this};
    const QDBusReply<QString> labelReply = QDBusConnection::sessionBus().call(labelMsg);
    if (!alive || !labelReply.isValid()) {
        qDebug() << "error getting display label via dbus:" << labelReply.error();
        co_return;
    }
    QString label = labelReply.value();
    qDebug() << "Adding display:" << label; // TODO: remove

    QDBusMessage maxBrightnessMsg = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                   u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                                   u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                                   u"maxBrightness"_s);
    maxBrightnessMsg << displayId;
    const QDBusReply<int> maxBrightnessReply = QDBusConnection::sessionBus().call(maxBrightnessMsg);
    if (!alive || !maxBrightnessReply.isValid()) {
        qDebug() << "error getting max screen brightness via dbus:" << maxBrightnessReply.error();
        co_return;
    }
    int maxBrightness = maxBrightnessReply.value();

    QDBusMessage minBrightnessMsg = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                   u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                                   u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                                   u"minBrightness"_s);
    minBrightnessMsg << displayId;
    const QDBusReply<int> minBrightnessReply = QDBusConnection::sessionBus().call(minBrightnessMsg);
    if (!alive || !minBrightnessReply.isValid()) {
        qDebug() << "error getting min screen brightness via dbus:" << minBrightnessReply.error();
        co_return;
    }
    int minBrightness = minBrightnessReply.value();

    QDBusMessage brightnessMsg = QDBusMessage::createMethodCall(u"org.kde.Solid.PowerManagement"_s,
                                                                u"/org/kde/Solid/PowerManagement/Actions/ScreenBrightnessControl"_s,
                                                                u"org.kde.Solid.PowerManagement.Actions.ScreenBrightnessControl"_s,
                                                                u"brightness"_s);
    brightnessMsg << displayId;
    const QDBusReply<int> brightnessReply = QDBusConnection::sessionBus().call(brightnessMsg);
    if (!alive || !brightnessReply.isValid()) {
        qDebug() << "error getting screen brightness via dbus:" << brightnessReply.error();
        co_return;
    }
    int brightness = brightnessReply.value();

    m_displays.appendDisplay(displayId, label, brightness, minBrightness, maxBrightness);
}

QBindable<bool> ScreenBrightnessControl::bindableIsBrightnessAvailable()
{
    return &m_isBrightnessAvailable;
}

#include "moc_screenbrightnesscontrol.cpp"
