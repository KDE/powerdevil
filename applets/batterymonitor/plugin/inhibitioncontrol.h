/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 * SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "applicationdata_p.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QObjectBindableProperty>
#include <QProperty>

#include <qqmlregistration.h>
#include <qtmetamacros.h>

#include <memory>

struct PolicyAgentInhibition {
    enum Flag {
        ConfiguredToSuppress = 0x1,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QString what;
    QString who;
    QString why;
    QString mode;
    uint flags;
};

class QDBusServiceWatcher;

class InhibitionControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<QVariantMap> inhibitions READ default NOTIFY inhibitionsChanged BINDABLE bindableInhibitions)
    Q_PROPERTY(QList<QVariantMap> suppressedInhibitions READ default NOTIFY suppressedInhibitionsChanged BINDABLE bindableSuppressedInhibitions)
    Q_PROPERTY(bool hasInhibition READ default NOTIFY hasInhibitionChanged BINDABLE bindableHasInhibition)
    Q_PROPERTY(bool isLidPresent READ default NOTIFY isLidPresentChanged BINDABLE bindableIsLidPresent)
    Q_PROPERTY(bool triggersLidAction READ default NOTIFY triggersLidActionChanged BINDABLE bindableTriggersLidAction)
    Q_PROPERTY(bool isManuallyInhibited READ default NOTIFY isManuallyInhibitedChanged BINDABLE bindableIsManuallyInhibited)
    Q_PROPERTY(bool isManuallyInhibitedError READ default WRITE default NOTIFY isManuallyInhibitedErrorChanged BINDABLE bindableIsManuallyInhibitedError)
    Q_PROPERTY(bool isSilent READ isSilent WRITE setIsSilent)

public:
    Q_INVOKABLE void inhibit(const QString &reason);
    Q_INVOKABLE void uninhibit();
    Q_INVOKABLE void suppressInhibition(const QString &appName, const QString &reason, bool permanently = false);
    Q_INVOKABLE void unsuppressInhibition(const QString &appName, const QString &reason, bool permanently = false);

    explicit InhibitionControl(QObject *parent = nullptr);
    ~InhibitionControl() override;

Q_SIGNALS:
    void inhibitionsChanged(const QList<QVariantMap> &inhibitions);
    void suppressedInhibitionsChanged(const QList<QVariantMap> &inhibitions);
    void hasInhibitionChanged(bool status);
    void isLidPresentChanged(bool status);
    void triggersLidActionChanged(bool status);
    void isManuallyInhibitedChanged(bool status);
    void isManuallyInhibitedErrorChanged(bool status);

private Q_SLOTS:
    void onServiceRegistered(const QString &serviceName);
    void onServiceUnregistered(const QString &serviceName);
    void onPolicyAgentPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);
    void onHasInhibitionChanged(bool status);
    void onIsManuallyInhibitedChanged(bool status);
    void onisManuallyInhibitedErrorChanged(bool status);

private:
    bool isSilent();
    void setIsSilent(bool status);
    void checkInhibitions();
    QList<QVariantMap> updatedInhibitions(const QList<PolicyAgentInhibition> &inhibitions);

    QBindable<QList<QVariantMap>> bindableInhibitions();
    QBindable<QList<QVariantMap>> bindableSuppressedInhibitions();
    QBindable<bool> bindableHasInhibition();
    QBindable<bool> bindableIsLidPresent();
    QBindable<bool> bindableTriggersLidAction();
    QBindable<bool> bindableIsManuallyInhibited();
    QBindable<bool> bindableIsManuallyInhibitedError();

    Q_OBJECT_BINDABLE_PROPERTY(InhibitionControl, QList<QVariantMap>, m_inhibitions, &InhibitionControl::inhibitionsChanged)
    Q_OBJECT_BINDABLE_PROPERTY(InhibitionControl, QList<QVariantMap>, m_suppressedInhibitions, &InhibitionControl::suppressedInhibitionsChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InhibitionControl, bool, m_hasInhibition, false, &InhibitionControl::hasInhibitionChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InhibitionControl, bool, m_isLidPresent, false, &InhibitionControl::isLidPresentChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InhibitionControl, bool, m_triggersLidAction, false, &InhibitionControl::triggersLidActionChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InhibitionControl, bool, m_isManuallyInhibited, false, &InhibitionControl::isManuallyInhibitedChanged)
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(InhibitionControl, bool, m_isManuallyInhibitedError, false, &InhibitionControl::isManuallyInhibitedErrorChanged)

    std::unique_ptr<QDBusServiceWatcher> m_solidWatcher;
    std::unique_ptr<QDBusServiceWatcher> m_fdoWatcher;
    bool m_isSilent = false;

    ApplicationData m_data;
};
