/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only

*/

#pragma once

#include <QDBusVariant>
#include <QObject>

#include "powerdevilcore_export.h"

namespace PowerDevil
{
/**
 * Tells whether the machine is a convertible that is currently held as a tablet rather than used
 * as a laptop, which is what the desktop portal answers for the session.
 *
 * The answer is asked for when this object is made and kept up to date from there on, so it reads
 * false until the first answer comes back.
 */
class POWERDEVILCORE_EXPORT TabletModeWatcher : public QObject
{
    Q_OBJECT

public:
    explicit TabletModeWatcher(QObject *parent = nullptr);
    ~TabletModeWatcher() override;

    /**
     * @returns whether the machine is currently held as a tablet.
     */
    bool isTabletMode() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the machine is turned from a laptop into a tablet or back.
     *
     * @param tabletMode Whether the machine is now held as a tablet
     */
    void tabletModeChanged(bool tabletMode);

private Q_SLOTS:
    void onPortalSettingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private:
    void setTabletMode(bool tabletMode);

    bool m_tabletMode = false;
};

}
