/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *   SPDX-FileCopyrightText: 2015 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KScreen/Types>

#include <powerdevilaction.h>
#include <powerdevilbackendinterface.h>

#include <optional>

namespace PowerDevil::BundledActions
{
class HandleButtonEvents : public PowerDevil::Action
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.HandleButtonEvents")

public:
    explicit HandleButtonEvents(QObject *parent);
    ~HandleButtonEvents() override;

    bool loadAction(const KConfigGroup &config) override;
    bool isSupported() override;

Q_SIGNALS:
    void triggersLidActionChanged(bool triggers);

protected:
    void triggerImpl(const QVariantMap &args) override;
    void onProfileUnload() override;
    void onIdleTimeout(std::chrono::milliseconds timeout) override;

public Q_SLOTS:
    int lidAction() const;
    bool triggersLidAction() const;

private Q_SLOTS:
    void onButtonPressed(PowerDevil::BackendInterface::ButtonType type);
    void onLidClosedChanged(bool closed);
    void powerOffButtonTriggered();
    void powerDownButtonTriggered();
    void suspendToRam();
    void suspendToDisk();

    void checkOutputs();

private:
    void processAction(uint action);
    void triggerAction(const QString &action, const QVariant &type);

    KScreen::ConfigPtr m_screenConfiguration;
    uint m_lidAction = 0;
    bool m_triggerLidActionWhenExternalMonitorPresent = false;
    bool m_externalMonitorPresent = false;

    uint m_powerButtonAction = 0;
    uint m_powerDownButtonAction = 0;
    uint m_sleepButtonAction = 1;
    uint m_hibernateButtonAction = 2;

    std::optional<int> m_oldKeyboardBrightness;
};

}
