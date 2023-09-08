/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <powerdevilaction.h>

namespace PowerDevil::BundledActions
{
class RunScript : public PowerDevil::Action
{
    Q_OBJECT

public:
    explicit RunScript(QObject *parent);
    ~RunScript() override;

protected:
    void onProfileUnload() override;
    void onIdleTimeout(std::chrono::milliseconds timeout) override;
    void onProfileLoad(const QString &previousProfile, const QString &newProfile) override;
    void triggerImpl(const QVariantMap &args) override;

public:
    bool loadAction(const KConfigGroup &config) override;

private:
    void runCommand();
    int m_scriptPhase;
    QString m_scriptCommand;
};

}
