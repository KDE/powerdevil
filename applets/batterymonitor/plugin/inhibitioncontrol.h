/*
 * SPDX-FileCopyrightText: 2024 Bohdan Onofriichuk <bogdan.onofriuchuk@gmail.com>
 * SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
 * SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>
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
#include <QtQml/qqmlregistration.h>

#include <memory>

struct PolicyAgentInhibition {
    enum Flag {
        Active = 0x1,
        Allowed = 0x2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QString what;
    QString who;
    QString why;
    QString mode;
    uint flags;
};

class QDBusServiceWatcher;

class RequestedInhibition
{
    Q_GADGET

    Q_PROPERTY(QString appName MEMBER appName CONSTANT FINAL)
    Q_PROPERTY(QString prettyName MEMBER prettyName CONSTANT FINAL)
    Q_PROPERTY(QString icon MEMBER icon CONSTANT FINAL)
    Q_PROPERTY(QString reason MEMBER reason CONSTANT FINAL)
    Q_PROPERTY(QStringList behaviors MEMBER behaviors CONSTANT FINAL)
    Q_PROPERTY(bool active MEMBER active CONSTANT FINAL)
    Q_PROPERTY(bool allowed MEMBER allowed CONSTANT FINAL)

public:
    bool operator==(const RequestedInhibition &) const = default;
    bool operator!=(const RequestedInhibition &) const = default;

    QString appName;
    QString prettyName;
    QString icon;
    QString reason;
    QStringList behaviors;
    bool active;
    bool allowed;
};

class InhibitionControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<RequestedInhibition> requestedInhibitions READ default NOTIFY requestedInhibitionsChanged BINDABLE bindableRequestedInhibitions)
    Q_PROPERTY(bool hasInhibition READ default NOTIFY hasInhibitionChanged BINDABLE bindableHasInhibition)
    Q_PROPERTY(bool isLidPresent READ default NOTIFY isLidPresentChanged BINDABLE bindableIsLidPresent)
    Q_PROPERTY(bool triggersLidAction READ default NOTIFY triggersLidActionChanged BINDABLE bindableTriggersLidAction)
    Q_PROPERTY(bool isManuallyInhibited READ default NOTIFY isManuallyInhibitedChanged BINDABLE bindableIsManuallyInhibited)
    Q_PROPERTY(bool isManuallyInhibitedError READ default WRITE default NOTIFY isManuallyInhibitedErrorChanged BINDABLE bindableIsManuallyInhibitedError)
    Q_PROPERTY(bool isSilent READ isSilent WRITE setIsSilent)

public:
    Q_INVOKABLE void inhibit(const QString &reason);
    Q_INVOKABLE void uninhibit();
    Q_INVOKABLE void setInhibitionAllowed(const QString &appName, const QString &reason, bool allowed);

    explicit InhibitionControl(QObject *parent = nullptr);
    ~InhibitionControl() override;

Q_SIGNALS:
    void requestedInhibitionsChanged(const QList<RequestedInhibition> &inhibitions);
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
    QList<RequestedInhibition> updatedInhibitions(const QList<PolicyAgentInhibition> &inhibitions);

    QBindable<QList<RequestedInhibition>> bindableRequestedInhibitions();
    QBindable<bool> bindableHasInhibition();
    QBindable<bool> bindableIsLidPresent();
    QBindable<bool> bindableTriggersLidAction();
    QBindable<bool> bindableIsManuallyInhibited();
    QBindable<bool> bindableIsManuallyInhibitedError();

    Q_OBJECT_BINDABLE_PROPERTY(InhibitionControl, QList<RequestedInhibition>, m_requestedInhibitions, &InhibitionControl::requestedInhibitionsChanged)
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
