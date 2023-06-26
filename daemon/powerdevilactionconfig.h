/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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

#pragma once

#include <QWidget>

#include <KConfigGroup>

#include "powerdevilcore_export.h"

namespace PowerDevil
{
/**
 * @brief The base class for Action's config interfaces
 *
 * This class should be reimplemented when creating a new Action. It is used to generate
 * a configuration UI for your action and integrate it into KDE Power Management System's
 * config module seamlessly.
 *
 * @par Creating an ActionConfig
 *
 * If you already have been through creating an Action, you have seen that Action's desktop
 * file contains already all the needed information for loading its respective ActionConfig. For this
 * reason, despite ActionConfig being a plugin, you do not need to create a desktop file for it -
 * you just have to write the correct info into the Action's one.
 *
 * @par The key/value pair rationale
 *
 * The config UI works in a key/value fashion, where value is a generic QWidget. This is done
 * to keep the UI consistent without hurting the flexibility of the implementation.
 * Please remember to keep the configuration as easy and essential as possible: if you have gone
 * beyond 2 rows, you might be doing something wrong.
 *
 * @par Loading and saving configuration
 *
 * Of course, you should also be providing the logic for saving and loading your action's configuration.
 * This is done through KConfigGroup: your action has a KConfigGroup assigned by KDE Power Management System
 * which can be accessed through configGroup or overwritten through setConfigGroup. The very same group
 * you are loading/saving here will be the one passed to Action::loadAction.
 *
 * @see Action
 *
 * @since 4.6
 */
class POWERDEVILCORE_EXPORT ActionConfig : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ActionConfig)

public:
    /**
     * Default constructor
     */
    explicit ActionConfig(QObject *parent);
    /**
     * Default destructor
     */
    ~ActionConfig() override;

    /**
     * @returns The KConfigGroup associated to this action, where data should be saved (and loaded)
     *
     * @note You should not track this KConfigGroup, as it might change from profile to profile.
     *       Always use this function to access it.
     */
    KConfigGroup configGroup() const;
    /**
     * Overwrites the config group associated with this Action with @c group.
     *
     * @param group A KConfigGroup carrying the settings for this Action.
     */
    void setConfigGroup(const KConfigGroup &group);

    /**
     * This function should return the key/value pairs containing your Action's configuration UI.
     *
     * @returns A list of QString, QWidget pairs representing your Action's configuration UI
     */
    virtual QList<QPair<QString, QWidget *>> buildUi() = 0;

    /**
     * This function gets called whenever the parent module requires to load a specific configuration. This
     * usually occurs when @c configGroup is updated, for example due to the fact that the user wants to modify
     * a different profile. In this function, you should update the info contained in the widgets generated
     * by @c buildUi accordingly.
     */
    virtual void load() = 0;
    /**
     * This function gets called whenever the parent module requires to save the configuration. Usually, you
     * want to read values from the widgets generated with @c buildUi and call @c setConfigGroup to update the
     * Action's configuration.
     */
    virtual void save() = 0;

protected Q_SLOTS:
    /**
     * Call this slot whenever the user makes some changes to the widgets.
     */
    void setChanged();

Q_SIGNALS:
    /**
     * This signal gets emitted every time the user changes the (unsaved) configuration. Do not emit this signal
     * directly: call setChanged instead.
     *
     * @see setChanged
     */
    void changed();

private:
    class Private;
    Private *const d;
};

}
